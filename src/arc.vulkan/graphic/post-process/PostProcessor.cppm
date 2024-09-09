module;

#include <vulkan/vulkan.h>

export module Graphic.PostProcessor;

export import Core.Vulkan.Context;
export import Core.Vulkan.Buffer.FrameBuffer;
export import Core.Vulkan.RenderProcedure;
export import Core.Vulkan.Buffer.CommandBuffer;
export import Core.Vulkan.CommandPool;
export import Core.Vulkan.Semaphore;
export import Core.Vulkan.Preinstall;

import Geom.Vector2D;
import std;

export namespace Graphic{
	struct AttachmentPort{
	    //OPTM merge these?
		std::unordered_map<std::uint32_t, VkImageView> in{};
		std::unordered_map<std::uint32_t, VkImageView> out{};
		std::unordered_map<std::uint32_t, VkImage> toTransferOwnership{};

		void addComputeInputAttachment(std::uint32_t index, const Core::Vulkan::Attachment& attachment){
			in.insert_or_assign(index, attachment.getView());
			toTransferOwnership.insert_or_assign(index, attachment.getImage());
		}
	};

	using PortProv = std::move_only_function<AttachmentPort()>;
	using DescriptorLayoutProv = std::move_only_function<std::vector<VkDescriptorSetLayout>()>;

	struct PostProcessor{
		const Core::Vulkan::Context* context{};

		AttachmentPort port{};
		PortProv portProv{};

		Core::Vulkan::CommandPool transientCommandPool{};
		Core::Vulkan::CommandPool commandPool{};

		Core::Vulkan::CommandBuffer commandBuffer{};

		[[nodiscard]] Core::Vulkan::TransientCommand obtainTransientCommand(const bool compute) const{
			return transientCommandPool.obtainTransient(
				compute ? context->device.getPrimaryComputeQueue() : context->device.getPrimaryGraphicsQueue());
		}

		/**
		 * @brief [Negative: before DrawCall, 0: DrawCall, positive: after DrawCall]
		 */
		std::map<int, Core::Vulkan::CommandBuffer> appendCommandBuffers{};

		[[nodiscard]] PostProcessor() = default;

		[[nodiscard]] explicit PostProcessor(
			const Core::Vulkan::Context& context, const bool isCompute,
			PortProv&& portProv = {})
			: context{&context},
			  portProv{std::move(portProv)},
			  transientCommandPool{
				  context.device,
				  isCompute ? context.computeFamily() : context.graphicFamily(),
				  VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
			  }, commandPool{
				  context.device,
				  isCompute ? context.computeFamily() : context.graphicFamily(),
				  VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
			  }, commandBuffer{context.device, commandPool}{
			if(this->portProv){
				port = this->portProv();
			}
		}

		void resize(Geom::USize2 size) = delete;

		[[nodiscard]] Geom::USize2 size() const = delete;

	protected:
		void submitToQueue(const std::vector<VkCommandBuffer>& commandBuffers, const bool isCompute) const{
			VkSubmitInfo info{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = 0,
				.pWaitSemaphores = nullptr,
				.pWaitDstStageMask = nullptr,
				.commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size()),
				.pCommandBuffers = commandBuffers.data(),
				.signalSemaphoreCount = 0,//1,
				.pSignalSemaphores = nullptr//processCompleteSemaphore.asData()
			};

			vkQueueSubmit(isCompute ? context->device.getPrimaryComputeQueue() : context->device.getPrimaryGraphicsQueue(), 1, &info, nullptr);
		}

		void submitCommand(bool isCompute) const{
			std::vector<VkCommandBuffer> commandBuffers{};
			commandBuffers.reserve(appendCommandBuffers.size() + 1);

			if(appendCommandBuffers.empty()){
				commandBuffers.push_back(commandBuffer.get());
			}else{
				auto lowerBound = appendCommandBuffers.lower_bound(0);

				std::ranges::transform(
					std::ranges::subrange{appendCommandBuffers.begin(), lowerBound} | std::views::values,
					std::back_inserter(commandBuffers), &Core::Vulkan::CommandBuffer::get);

				commandBuffers.push_back(commandBuffer);

				std::ranges::transform(
					std::ranges::subrange{lowerBound, appendCommandBuffers.end()} | std::views::values,
					std::back_inserter(commandBuffers), &Core::Vulkan::CommandBuffer::get);
			}

			submitToQueue(commandBuffers, isCompute);
		}
	};

	struct ComputePostProcessor : PostProcessor{
		Core::Vulkan::SinglePipelineData pipelineData{};
		std::vector<Core::Vulkan::Attachment> images{};

		std::vector<Core::Vulkan::UniformBuffer> uniformBuffers{};
		std::vector<Core::Vulkan::DescriptorBuffer> descriptorBuffers{};

		DescriptorLayoutProv layoutProv{};

		std::move_only_function<void(ComputePostProcessor&)> commandRecorder{};
		std::move_only_function<void(VkCommandBuffer)> commandRecorderAdditional{};
		std::move_only_function<void(ComputePostProcessor&)> descriptorSetUpdator{};
		std::move_only_function<void(ComputePostProcessor&)> appendCommandRecorder{};
		std::move_only_function<void(ComputePostProcessor&)> resizeCallback{};

		[[nodiscard]] ComputePostProcessor() = default;

		[[nodiscard]] explicit ComputePostProcessor(const Core::Vulkan::Context& context, PortProv&& portProv, DescriptorLayoutProv&& layoutProv = {})
		: PostProcessor{context, true, std::move(portProv)}, pipelineData{&context}, layoutProv{std::move(layoutProv)}{

		}

		template <std::derived_from<Core::Vulkan::Attachment> T = Core::Vulkan::Attachment, std::invocable<ComputePostProcessor&, T&> Func>
		void addImage(Func&& creator){
			T image{context->physicalDevice, context->device};
			creator(*this, image);

			images.push_back(std::move(static_cast<Core::Vulkan::Attachment&&>(image)));
		}

		void bindAppendedDescriptors(VkCommandBuffer commandBuffer){
			commandRecorderAdditional(commandBuffer);
		}

		void createDescriptorBuffers(const std::uint32_t size){
			descriptorBuffers.reserve(size);
			for(std::size_t i = 0; i < size; ++i){
				descriptorBuffers.push_back(Core::Vulkan::DescriptorBuffer{
					context->physicalDevice, context->device, pipelineData.descriptorSetLayout, pipelineData.descriptorSetLayout.size()
				});
			}
		}

		void addUniformBuffer(const std::size_t size, const VkBufferUsageFlags otherUsages = 0){
			uniformBuffers.push_back(Core::Vulkan::UniformBuffer{context->physicalDevice, context->device, size, otherUsages});
		}

		template <typename T>
			requires (std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
		void addUniformBuffer(const T& data, const VkBufferUsageFlags otherUsages = 0){
			uniformBuffers.push_back(Core::Vulkan::UniformBuffer{context->physicalDevice, context->device, sizeof(T), otherUsages});
			uniformBuffers.back().memory.loadData<T>(data, 0);
		}

		std::vector<VkDescriptorSetLayout> getAppendedLayout(){
			if(layoutProv)return layoutProv();
			return {};
		}

		[[nodiscard]] Geom::USize2 size() const{
			return pipelineData.size;
		}

		void updateDescriptors(){
			if(descriptorSetUpdator)descriptorSetUpdator(*this);
		}

		void recordCommand(){
			if(commandRecorder)commandRecorder(*this);
		}

		void resize(Geom::USize2 size){
			if(size == this->size())return;
			pipelineData.size = size;
			if(resizeCallback)resizeCallback(*this);

			auto command = obtainTransientCommand(true);
			for (auto && localAttachment : images){
				localAttachment.resize(size, command);
			}
			command = {};

			if(portProv){
				port = portProv();
			}

			updateDescriptors();
			recordCommand();
		}

		void submitCommand() const{
			PostProcessor::submitCommand(true);
		}
	};

	struct GraphicPostProcessor : PostProcessor{
		//Static
		Core::Vulkan::FramebufferLocal framebuffer{};
		Core::Vulkan::RenderProcedure renderProcedure{};


		/**
		 * @brief [Negative: before DrawCall, 0: DrawCall, positive: after DrawCall]
		 */
		std::function<void(GraphicPostProcessor&)> commandRecorder{};
		std::function<void(GraphicPostProcessor&)> descriptorSetUpdator{};
		std::function<void(GraphicPostProcessor&)> appendCommandRecorder{};
		std::function<void(GraphicPostProcessor&)> resizeCallback{};


		[[nodiscard]] GraphicPostProcessor() = default;

		[[nodiscard]] explicit GraphicPostProcessor(const Core::Vulkan::Context& context, PortProv&& portProv)
		: PostProcessor{context, false, std::move(portProv)}{
			renderProcedure.init(context);
		}

		[[nodiscard]] Core::Vulkan::TransientCommand obtainTransientCommand() const{
			return PostProcessor::obtainTransientCommand(false);
		}

		/**
		 * @param initFunc Used To set local attachments
		 */
		void createFramebuffer(std::regular_invocable<Core::Vulkan::FramebufferLocal&> auto&& initFunc){
			framebuffer = Core::Vulkan::FramebufferLocal{context->device, renderProcedure.size, renderProcedure.renderPass};

			initFunc(framebuffer);

			framebuffer.loadCapturedAttachments(0);
			framebuffer.loadExternalImageView(port.in);
			framebuffer.loadExternalImageView(port.out);
			framebuffer.create();
		}

		[[nodiscard]] Geom::USize2 size() const{
			return renderProcedure.size;
		}

		void resize(Geom::USize2 size){
			if(resizeCallback)resizeCallback(*this);
			renderProcedure.resize(size);
			framebuffer.setRenderPass(renderProcedure.renderPass);

			if(portProv){
				port = portProv();
			}

			framebuffer.resize(
				size,
				transientCommandPool.obtainTransient(context->device.getPrimaryGraphicsQueue()),
				port.in, port.out);

			updateDescriptors();
			recordCommand();
		}

		void updateDescriptors(){
			descriptorSetUpdator(*this);
		}

		void recordCommand(){
			commandRecorder(*this);
		}

		void submitCommand() const{
			PostProcessor::submitCommand(false);
		}
	};

	struct GraphicPostProcessorFactory{
		const Core::Vulkan::Context* context{};
		std::function<GraphicPostProcessor(const GraphicPostProcessorFactory&, PortProv&&, Geom::USize2)> creator{};

		GraphicPostProcessor generate(const Geom::USize2 size, PortProv&& portProv) const{
			return creator(*this, std::move(portProv), size);
		}
	};

	struct ComputePostProcessorFactory{
		const Core::Vulkan::Context* context{};
		std::function<ComputePostProcessor(const ComputePostProcessorFactory&, PortProv&&, DescriptorLayoutProv&&, Geom::USize2)> creator{};

		ComputePostProcessor generate(const Geom::USize2 size, PortProv&& portProv, DescriptorLayoutProv&& layoutProv = {}) const{
			return creator(*this, std::move(portProv), std::move(layoutProv), size);
		}
	};
}

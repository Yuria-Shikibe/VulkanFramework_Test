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
	};

	using PortProv = std::function<AttachmentPort()>;

	struct PostProcessor{
		const Core::Vulkan::Context* context{};

		AttachmentPort port{};
		PortProv portProv{};

		//TODO uses a pool in some global place??
		Core::Vulkan::CommandPool transientCommandPool{};
		Core::Vulkan::CommandPool commandPool{};

		Core::Vulkan::CommandBuffer commandBuffer{};



		[[nodiscard]] Core::Vulkan::TransientCommand obtainTransientCommand(const bool compute) const{
			return transientCommandPool.obtainTransient(
				compute ? context->device.getComputeQueue() : context->device.getGraphicsQueue());
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
				  isCompute ? context.physicalDevice.queues.computeFamily : context.physicalDevice.queues.graphicsFamily,
				  VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
			  }, commandPool{
				  context.device,
				  isCompute ? context.physicalDevice.queues.computeFamily : context.physicalDevice.queues.graphicsFamily,
				  VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
			  }, commandBuffer{context.device, commandPool}{
			if(this->portProv){
				port = this->portProv();
			}
		}

		void resize(Geom::USize2 size) = delete;

		[[nodiscard]] Geom::USize2 size() const = delete;
	};

	struct ComputePostProcessor : PostProcessor{
		Core::Vulkan::PipelineData pipelineData{};
		std::vector<Core::Vulkan::Attachment> images{};

		std::function<void(ComputePostProcessor&)> commandRecorder{};
		std::function<void(ComputePostProcessor&)> descriptorSetUpdator{};
		std::function<void(ComputePostProcessor&)> appendCommandRecorder{};
		std::function<void(ComputePostProcessor&)> resizeCallback{};

		[[nodiscard]] ComputePostProcessor() = default;

		[[nodiscard]] explicit ComputePostProcessor(const Core::Vulkan::Context& context, PortProv&& portProv)
		: PostProcessor{context, true, std::move(portProv)}, pipelineData{&context}{

		}

		[[nodiscard]] Geom::USize2 size() const{
			return pipelineData.size;
		}

		void updateDescriptors(){
			descriptorSetUpdator(*this);
		}

		void recordCommand(){
			commandRecorder(*this);
		}

		void resize(Geom::USize2 size){
			if(size == this->size())return;
			if(resizeCallback)resizeCallback(*this);
			pipelineData.resize(size);

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
				transientCommandPool.obtainTransient(context->device.getGraphicsQueue()),
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

	    template <std::ranges::range Rng = std::initializer_list<VkSemaphore>>
	        requires (std::convertible_to<VkSemaphore, std::ranges::range_value_t<Rng>>)
		void submitCommand(Rng&& toWait, const VkPipelineStageFlags* stageFlags = Core::Vulkan::Seq::StageFlagBits<VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT>) const{
			std::vector<VkCommandBuffer> commandBuffers{};
			commandBuffers.reserve(appendCommandBuffers.size() + 1);

		    std::vector<VkSemaphore> semaphores{};
		    std::ranges::transform(toWait, std::back_inserter(semaphores), [](const auto& s){return static_cast<VkSemaphore>(s);});

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

			context->commandSubmit_Graphics(VkSubmitInfo{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = static_cast<std::uint32_t>(semaphores.size()),
				.pWaitSemaphores = semaphores.data(),
				.pWaitDstStageMask = stageFlags,
				.commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size()),
				.pCommandBuffers = commandBuffers.data(),
				.signalSemaphoreCount = 0,//1,
				.pSignalSemaphores = nullptr//processCompleteSemaphore.asData()
			}, nullptr);
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
		std::function<ComputePostProcessor(const ComputePostProcessorFactory&, PortProv&&, Geom::USize2)> creator{};

		ComputePostProcessor generate(const Geom::USize2 size, PortProv&& portProv) const{
			return creator(*this, std::move(portProv), size);
		}
	};
}

module;

#include <vulkan/vulkan.h>

export module Graphic.PostProcessor;

export import Core.Vulkan.Context;
export import Core.Vulkan.Buffer.FrameBuffer;
export import Core.Vulkan.RenderPassGroup;
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

	struct PostProcessor{
		const Core::Vulkan::Context* context{};

		/**
		 * @brief Should be the only dynamic part of each similar post processor
		 */
		AttachmentPort port{};

	    //TODO uses a pool in some global place??
		Core::Vulkan::CommandPool transientCommandPool{};
		Core::Vulkan::CommandPool commandPool{};

		//Static
		Core::Vulkan::FramebufferLocal framebuffer{};
		Core::Vulkan::RenderProcedure renderProcedure{};

		Core::Vulkan::CommandBuffer commandBuffer{};


		/**
		 * @brief [Negative: before DrawCall, 0: DrawCall, positive: after DrawCall]
		 */
		std::map<int, Core::Vulkan::CommandBuffer> appendCommandBuffers{};

		Core::Vulkan::Semaphore processCompleteSemaphore{};

		std::function<void(PostProcessor&)> commandRecorder{};
		std::function<void(PostProcessor&)> appendCommandRecorder{};
		std::function<void(PostProcessor&)> descriptorSetUpdator{};
		std::function<void(PostProcessor&)> resizeCallback{};

		using PortProv = std::function<AttachmentPort()>;
		PortProv portProv{};


		[[nodiscard]] PostProcessor() = default;

		[[nodiscard]] explicit PostProcessor(const Core::Vulkan::Context& context, PortProv&& portProv) : context{&context}, portProv{std::move(portProv)}{
			transientCommandPool = Core::Vulkan::CommandPool{
					context.device, context.physicalDevice.queues.graphicsFamily,
					VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
				};

			commandPool = Core::Vulkan::CommandPool{
					context.device, context.physicalDevice.queues.graphicsFamily,
					VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
				};

			commandBuffer = commandPool.obtain();

			if(this->portProv){
				port = this->portProv();
			}

			renderProcedure.init(context);
			processCompleteSemaphore = Core::Vulkan::Semaphore{context.device};
		}

		[[nodiscard]] Core::Vulkan::TransientCommand obtainTransientCommand() const{
			return transientCommandPool.obtainTransient(context->device.getGraphicsQueue());
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
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = processCompleteSemaphore.asData()
			}, nullptr);
		}

		[[nodiscard]] VkSemaphore getToWait() const{
			return processCompleteSemaphore.get();
		}
	};

	struct PostProcessorFactory{
		const Core::Vulkan::Context* context{};
		std::function<PostProcessor(const PostProcessorFactory&, PostProcessor::PortProv&&, Geom::USize2)> creator{};

		PostProcessor generate(const Geom::USize2 size, PostProcessor::PortProv&& portProv) const{
			return creator(*this, std::move(portProv), size);
		}
	};
}

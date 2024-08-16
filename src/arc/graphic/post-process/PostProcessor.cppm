module;

#include <vulkan/vulkan.h>

export module Graphic.PostProcessor;

import Core.Vulkan.Context;
import Core.Vulkan.Buffer.FrameBuffer;
import Core.Vulkan.RenderPassGroup;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.CommandPool;
import Core.Vulkan.Semaphore;

import Geom.Vector2D;
import std;

export namespace Graphic{
	struct Port{
		std::unordered_map<std::uint32_t, VkImageView> in{};
		std::unordered_map<std::uint32_t, VkImageView> out{};
	};

	struct PostProcessor{
		Core::Vulkan::Context* context{};

		//Dynamic rebindable
		Port port{};

		Core::Vulkan::CommandPool transientCommandPool{};

		//Static
		Core::Vulkan::FramebufferLocal framebuffer{};
		Core::Vulkan::RenderProcedure renderProcedure{};

		Core::Vulkan::CommandBuffer commandBuffer{};

		Core::Vulkan::Semaphore processCompleteSemaphore{};

		std::function<void(PostProcessor&)> commandRecorder{};

		explicit PostProcessor(Core::Vulkan::Context& context) : context{&context}{
			transientCommandPool = Core::Vulkan::CommandPool{
					context.device, context.physicalDevice.queues.graphicsFamily,
					VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
				};

			framebuffer.setDevice(context.device);
			renderProcedure.init(context);
			processCompleteSemaphore = Core::Vulkan::Semaphore{context.device};
		}

		/**
		 * @param initFunc Used To set local attachments
		 */
		template <std::regular_invocable<Core::Vulkan::FramebufferLocal&> InitFunc>
		void createFramebuffer(InitFunc&& initFunc){
			initFunc(framebuffer);

			framebuffer.loadExternalImageView(port.in);
			framebuffer.loadExternalImageView(port.out);

			framebuffer.createFrameBuffer(renderProcedure.renderPass);
		}

		[[nodiscard]] Geom::USize2 size() const{
			return renderProcedure.size;
		}

		void resize(Geom::USize2 size){
			renderProcedure.resize(size);
			framebuffer.resize(
				size,
				transientCommandPool.obtainTransient(context->device.getGraphicsQueue()),
				port.in, port.out);
		}

		void recordCommand(){
			commandRecorder(*this);
		}

		void submitCommand(VkSemaphore toWait) const{
			context->commandSubmit_Graphics(
				commandBuffer, toWait, processCompleteSemaphore, nullptr
			);
		}
	};
}

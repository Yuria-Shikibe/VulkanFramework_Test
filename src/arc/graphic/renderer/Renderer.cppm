module;

#include <vulkan/vulkan.h>

export module Graphic.Renderer;

export import Core.Vulkan.CommandPool;
export import Core.Vulkan.Context;

export import Geom.Vector2D;
export import Geom.Rect_Orthogonal;
export import Graphic.PostProcessor;

import std;

export namespace Graphic{

	struct BasicRenderer{
	protected:
		Geom::USize2 size{};
		Core::Vulkan::CommandPool commandPool{};
		Core::Vulkan::CommandPool commandPool_Compute{};
		AttachmentPort port{};

	public:
		[[nodiscard]] BasicRenderer() = default;

		[[nodiscard]] explicit BasicRenderer(const Core::Vulkan::Context& context)
			:
			commandPool{context.device, context.graphicFamily(), 0},
			commandPool_Compute{context.device, context.computeFamily(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT}{}

		[[nodiscard]] Geom::USize2 getSize() const noexcept{ return size; }

		[[nodiscard]] const Core::Vulkan::CommandPool& getCommandPool() const noexcept{ return commandPool; }

		[[nodiscard]] const Core::Vulkan::Context& context() const noexcept = delete;

		void resize(Geom::USize2 size2) = delete;

		void init(Geom::USize2 size2) = delete;

		AttachmentPort& getPort() noexcept{
			return port;
		}

	protected:
		void resetCommandPool() const{
			commandPool.resetAll();
		}

		void setPort() = delete; // NOLINT(*-use-equals-delete)
	};
}

module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.FrameBuffer;

import Core.Vulkan.Dependency;
import Core.Vulkan.Concepts;

import std;
import Geom.Vector2D;

export namespace Core::Vulkan{
	class FrameBuffer : public Wrapper<VkFramebuffer>{
		Dependency<VkDevice> device{};

		std::vector<VkImageView> attachments{};

	public:
		~FrameBuffer(){
			if(device)vkDestroyFramebuffer(device, handle, nullptr);
		}

		[[nodiscard]] FrameBuffer() = default;

		FrameBuffer(const FrameBuffer& other) = delete;

		FrameBuffer(FrameBuffer&& other) noexcept = default;

		FrameBuffer& operator=(const FrameBuffer& other) = delete;

		FrameBuffer& operator=(FrameBuffer&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroyFramebuffer(device, handle, nullptr);
			Wrapper<VkFramebuffer>::operator =(std::move(other));
			device = std::move(other.device);
			return *this;
		}

		FrameBuffer(VkDevice device, const VkFramebufferCreateInfo& createInfo)
			: device{device}, attachments{std::ranges::begin(attachments), std::ranges::end(attachments)}{
			if(vkCreateFramebuffer(device, &createInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create framebuffer!");
			}
		}

		FrameBuffer(VkDevice device, const Geom::USize2 size, VkRenderPass renderPass, ContigiousRange<VkImageView> auto&& attachments, const std::uint32_t layers = 1)
			: device{device}, attachments{std::ranges::begin(attachments), std::ranges::end(attachments)}{
			if(size.area() == 0){
				throw std::invalid_argument("Invalid Size");
			}

			const VkFramebufferCreateInfo framebufferInfo{
					.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.renderPass = renderPass,
					.attachmentCount = static_cast<std::uint32_t>(std::ranges::size(attachments)),
					.pAttachments = std::ranges::data(attachments),
					.width = size.x,
					.height = size.y,
					.layers = layers
				};

			if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create framebuffer!");
			}
		}
	};
}

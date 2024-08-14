module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Attachment;

import Core.Vulkan.Image;

export namespace Core::Vulkan{
	struct Attachment{
		Image image{};
		ImageView defaultView{};

		[[nodiscard]] Attachment() = default;

		[[nodiscard]] Attachment(
			VkPhysicalDevice physicalDevice, VkDevice device,
			VkMemoryPropertyFlags properties,
			const VkImageCreateInfo& imageInfo, const VkImageViewCreateInfo& createInfo)
			: image{physicalDevice, device, properties, imageInfo}, defaultView{device, createInfo}{

		}
	};
}
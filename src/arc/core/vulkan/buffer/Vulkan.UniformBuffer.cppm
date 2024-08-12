module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.UniformBuffer;

import Core.Vulkan.Buffer.ExclusiveBuffer;
import std;

export namespace Core::Vulkan{
	struct UniformBuffer : ExclusiveBuffer{
		using ExclusiveBuffer::ExclusiveBuffer;

		[[nodiscard]] UniformBuffer(VkPhysicalDevice physicalDevice, VkDevice device, const VkDeviceSize size)
			: ExclusiveBuffer(physicalDevice, device, size,
			                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
			                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT){}

		[[nodiscard]] constexpr VkDescriptorBufferInfo getDescriptorInfo() const & noexcept{
			return {
					.buffer = handle,
					.offset = 0,
					.range = memory.size()
				};
		}
	};


}

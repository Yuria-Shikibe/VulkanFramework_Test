module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.UniformBuffer;

import Core.Vulkan.Buffer.PersistentTransferBuffer;
import std;

export namespace Core::Vulkan{
	struct UniformBuffer : PersistentTransferBuffer{
		using PersistentTransferBuffer::PersistentTransferBuffer;

		[[nodiscard]] UniformBuffer(VkPhysicalDevice physicalDevice, VkDevice device, const VkDeviceSize size)
			: PersistentTransferBuffer(physicalDevice, device, size,
			                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT){}

		[[nodiscard]] constexpr VkDescriptorBufferInfo getDescriptorInfo() const & noexcept{
			return {
					.buffer = handle,
					.offset = 0,
					.range = memory.size()
				};
		}
	};


}

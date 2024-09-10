module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.UniformBuffer;

import Core.Vulkan.Buffer.PersistentTransferBuffer;
import std;

export namespace Core::Vulkan{
	struct UniformBufferBase{
	protected:
		std::size_t uniformBlockSize{};

	public:
		[[nodiscard]] UniformBufferBase() = default;

		[[nodiscard]] explicit UniformBufferBase(std::size_t uniformBlockSize)
			: uniformBlockSize{uniformBlockSize}{}

		[[nodiscard]] std::size_t requestedSize() const noexcept{
			return uniformBlockSize;
		}

		[[nodiscard]] VkDescriptorBufferInfo getDescriptorInfo() const noexcept = delete;
	};

	struct UniformBuffer : UniformBufferBase, PersistentTransferBuffer{
		[[nodiscard]] UniformBuffer() = default;

		[[nodiscard]] UniformBuffer(VkPhysicalDevice physicalDevice, VkDevice device, const VkDeviceSize uniformBlockSize,
		                            const VkBufferUsageFlags otherUsages = 0u)
			:
			UniformBufferBase{uniformBlockSize},
			PersistentTransferBuffer(
				physicalDevice, device,
				uniformBlockSize,
				otherUsages | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT){
			setAddress();
		}

		[[nodiscard]] constexpr VkDescriptorBufferInfo getDescriptorInfo() const noexcept{
			return {
					.buffer = handle,
					.offset = 0,
					.range = memory.size()
				};
		}
	};

	struct UniformBuffer_Double : UniformBufferBase, PersistentTransferDoubleBuffer{
		[[nodiscard]] UniformBuffer_Double() = default;

		[[nodiscard]] UniformBuffer_Double(VkPhysicalDevice physicalDevice, VkDevice device,
		                                   const VkDeviceSize uniformBlockSize)
			: UniformBufferBase{uniformBlockSize},
			  PersistentTransferDoubleBuffer(physicalDevice, device, uniformBlockSize,
			                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT){
			targetBuffer.setAddress();
		}

		[[nodiscard]] VkDescriptorBufferInfo getDescriptorInfo() const noexcept{
			return {
					.buffer = targetBuffer,
					.offset = 0,
					.range = targetBuffer.memorySize()
				};
		}
	};
}

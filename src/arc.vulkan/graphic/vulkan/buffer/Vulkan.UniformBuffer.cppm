module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.UniformBuffer;

import Core.Vulkan.Buffer.PersistentTransferBuffer;
import std;

export namespace Core::Vulkan{
	struct UniformBuffer : PersistentTransferBuffer{
		using PersistentTransferBuffer::PersistentTransferBuffer;
	private:
		std::size_t uniformBlockSize{};

	public:
		[[nodiscard]] std::size_t requestedSize() const noexcept{
			return uniformBlockSize;
		}

		[[nodiscard]] UniformBuffer(VkPhysicalDevice physicalDevice, VkDevice device, const VkDeviceSize uniformBlockSize, const VkBufferUsageFlags otherUsages = 0u)
			: PersistentTransferBuffer(physicalDevice, device, uniformBlockSize,
			                 otherUsages | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT), uniformBlockSize{uniformBlockSize}{
			setAddress();
		}

		[[nodiscard]] constexpr VkDescriptorBufferInfo getDescriptorInfo() const & noexcept{
			return {
					.buffer = handle,
					.offset = 0,
					.range = memory.size()
				};
		}
	};

	struct UniformBuffer_Double : PersistentTransferDoubleBuffer{
		using PersistentTransferDoubleBuffer::PersistentTransferDoubleBuffer;
	private:
		std::size_t uniformBlockSize{};

	public:
		[[nodiscard]] std::size_t requestedSize() const noexcept{
			return uniformBlockSize;
		}

		[[nodiscard]] UniformBuffer_Double(VkPhysicalDevice physicalDevice, VkDevice device, const VkDeviceSize uniformBlockSize)
			: PersistentTransferDoubleBuffer(physicalDevice, device, uniformBlockSize,
							  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT), uniformBlockSize{uniformBlockSize}{
			targetBuffer.setAddress();
		}

		[[nodiscard]] VkDescriptorBufferInfo getDescriptorInfo() const & noexcept{
			return {
				.buffer = targetBuffer,
				.offset = 0,
				.range = targetBuffer.memorySize()
			};
		}
	};
}

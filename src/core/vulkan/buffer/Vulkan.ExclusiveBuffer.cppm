module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.ExclusiveBuffer;

export import Core.Vulkan.Buffer;
import Core.Vulkan.Dependency;
import Core.Vulkan.Buffer.CommandBuffer;
import std;

export namespace Core::Vulkan{
	void copyBuffer(
		VkCommandPool commandPool, VkDevice device, VkQueue queue,
		VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size
	){
		TransientCommand commandBuffer{device, commandPool, queue};

		VkBufferCopy copyRegion{.size = size};
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
	}

	/**
	 * @brief owns the whole memory
	 */
	class ExclusiveBuffer : public BasicBuffer{

	public:
		using BasicBuffer::BasicBuffer;



		template <std::invocable<VkDevice, ExclusiveBuffer&> InitFunc>
		[[nodiscard]] explicit ExclusiveBuffer(VkDevice device, InitFunc initFunc) : BasicBuffer{device}{
			initFunc(device, *this);
		}

		[[nodiscard]] ExclusiveBuffer(
			VkPhysicalDevice physicalDevice, VkDevice device,
			VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties
		) : BasicBuffer{device, nullptr, size}{
			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = usage;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if(vkCreateBuffer(device, &bufferInfo, nullptr, &handler) != VK_SUCCESS){
				throw std::runtime_error("Failed to create buffer!");
			}

			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(device, handler, &memRequirements);

			VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, physicalDevice, properties);

			if(vkAllocateMemory(device, &allocInfo, nullptr, &memory.handler) != VK_SUCCESS){
				throw std::runtime_error("Failed to allocate buffer memory!");
			}

			vkBindBufferMemory(device, handler, memory, 0);
		}

		~ExclusiveBuffer(){
			if(device)vkFreeMemory(device, memory, nullptr);
		}

		ExclusiveBuffer(const ExclusiveBuffer& other) = delete;

		ExclusiveBuffer(ExclusiveBuffer&& other) noexcept = default;

		ExclusiveBuffer& operator=(const ExclusiveBuffer& other) = delete;

		ExclusiveBuffer& operator=(ExclusiveBuffer&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkFreeMemory(device, memory, nullptr);
			BasicBuffer::operator=(std::move(other));

			return *this;
		}

	};
}

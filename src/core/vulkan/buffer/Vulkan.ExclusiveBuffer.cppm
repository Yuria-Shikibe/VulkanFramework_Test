module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.ExclusiveBuffer;

import Core.Vulkan.Memory;
import Core.Vulkan.Dependency;
import Core.Vulkan.Buffer.CommandBuffer;
import std;

export namespace Core::Vulkan{
	namespace Util{
		void copyBuffer(
			VkCommandPool commandPool, VkDevice device, VkQueue queue,
			VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size
		){
			TransientCommand commandBuffer{device, commandPool, queue};

			VkBufferCopy copyRegion{.size = size};
			vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
		}

		void cmdCopyBuffer(
			const CommandBuffer& commandBuffer,
			VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size
		){

			VkBufferCopy copyRegion{.size = size};
			vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
		}
	}



	/**
	 * @brief owns the whole memory
	 */
	class ExclusiveBuffer : public Wrapper<VkBuffer>{
	public:
		DeviceMemory memory{};

		template <std::invocable<VkDevice, ExclusiveBuffer&> InitFunc>
		[[nodiscard]] explicit ExclusiveBuffer(VkDevice device, InitFunc initFunc) : memory{device}{
			initFunc(device, *this);
		}

		[[nodiscard]] ExclusiveBuffer() = default;

		[[nodiscard]] ExclusiveBuffer(
			VkPhysicalDevice physicalDevice, VkDevice device,
			VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties
		) : memory{device, properties}{
			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = usage;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if(vkCreateBuffer(device, &bufferInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create buffer!");
			}

			memory.allocate(physicalDevice, handle, size);

			vkBindBufferMemory(device, handle, memory, 0);
		}

		~ExclusiveBuffer(){
			if(memory.getDevice())vkDestroyBuffer(memory.getDevice(), handle, nullptr);
		}

		ExclusiveBuffer(const ExclusiveBuffer& other) = delete;

		ExclusiveBuffer(ExclusiveBuffer&& other) noexcept = default;

		ExclusiveBuffer& operator=(const ExclusiveBuffer& other) = delete;

		ExclusiveBuffer& operator=(ExclusiveBuffer&& other) noexcept{
			if(this == &other) return *this;
			if(memory.getDevice())vkDestroyBuffer(memory.getDevice(), handle, nullptr);

			Wrapper::operator =(std::move(other));
			memory = std::move(other.memory);
			return *this;
		}
	};

	class BufferView : public Wrapper<VkBufferView>{
		Dependency<VkDevice> device{};
		
	public:
		BufferView() = default;
		
		~BufferView(){
			if(device)vkDestroyBufferView(device, handle, nullptr);
		}

		BufferView(const BufferView& other) = delete;

		BufferView(BufferView&& other) noexcept = default;

		BufferView& operator=(const BufferView& other) = delete;

		BufferView& operator=(BufferView&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroyBufferView(device, handle, nullptr);
			
			Wrapper<VkBufferView>::operator =(std::move(other));
			device = std::move(other.device);
			return *this;
		}

		BufferView(VkDevice device, VkBufferViewCreateInfo createInfo) : device{device}{
			createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
			if(vkCreateBufferView(device, &createInfo, nullptr, &handle)){
				throw std::runtime_error("Failed to create buffer view!");
			}
		}

		BufferView(VkDevice device, VkBuffer buffer, VkFormat format,
			VkDeviceSize offset = 0, VkDeviceSize range = 0 /*VkBufferViewCreateFlags flags*/)
			: BufferView{
				device, {
					.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.buffer = buffer,
					.format = format,
					.offset = offset,
					.range = range
				}
			}{}
	};

	class StagingBuffer : public ExclusiveBuffer{
	public:
		using ExclusiveBuffer::ExclusiveBuffer;

		[[nodiscard]] StagingBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size)
			: ExclusiveBuffer{physicalDevice, device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT}{}
	};
}

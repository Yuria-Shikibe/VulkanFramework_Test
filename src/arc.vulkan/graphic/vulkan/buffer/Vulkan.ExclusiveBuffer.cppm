module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.ExclusiveBuffer;

import Core.Vulkan.Memory;
import ext.handle_wrapper;
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

		void copyBuffer(
			VkCommandBuffer commandBuffer,
			VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size
		){

			VkBufferCopy copyRegion{.size = size};
			vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
		}
	}



	/**
	 * @brief owns the whole memory
	 */
	class ExclusiveBuffer : public ext::wrapper<VkBuffer>{
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

			memory.acquireLimit(physicalDevice);

			memory.allocate(physicalDevice, handle, size);

			vkBindBufferMemory(device, handle, memory, 0);
		}

		[[nodiscard]] VkDevice getDevice() const{
			return memory.getDevice();
		}

		[[nodiscard]] auto size() const noexcept{
			return memory.size();
		}

		~ExclusiveBuffer(){
			if(memory.getDevice())vkDestroyBuffer(memory.getDevice(), handle, nullptr);
		}

		ExclusiveBuffer(const ExclusiveBuffer& other) = delete;

		ExclusiveBuffer(ExclusiveBuffer&& other) noexcept = default;

		ExclusiveBuffer& operator=(const ExclusiveBuffer& other) = delete;

		ExclusiveBuffer& operator=(ExclusiveBuffer&& other) noexcept = default;

		void copyBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize size) const{
			const VkBufferCopy copyRegion{.size = size};
			vkCmdCopyBuffer(commandBuffer, handle, dstBuffer, 1, &copyRegion);
		}

		void copyBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer) const{
			copyBuffer(commandBuffer, dstBuffer, memory.size());
		}

		[[nodiscard]] VkDeviceAddress getBufferAddress() const{
			const VkBufferDeviceAddressInfo bufferDeviceAddressInfo{
				.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
				.pNext = nullptr,
				.buffer = handle
			};
			return vkGetBufferDeviceAddress(getDevice(), &bufferDeviceAddressInfo);
		}
	};

	class BufferView : public ext::wrapper<VkBufferView>{
		ext::dependency<VkDevice> device{};
		
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
			
			wrapper<VkBufferView>::operator =(std::move(other));
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

		[[nodiscard]] StagingBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
			: ExclusiveBuffer{physicalDevice, device, size, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT}{}
	};
}

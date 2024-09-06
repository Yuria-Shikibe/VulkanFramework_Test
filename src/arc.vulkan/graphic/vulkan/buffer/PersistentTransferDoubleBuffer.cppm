module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.PersistentTransferBuffer;

import Core.Vulkan.Buffer.ExclusiveBuffer;
import std;

export namespace Core::Vulkan{
	struct PersistentBase{
	protected:
		void* mappedData{};

	public:
		[[nodiscard]] std::byte* getMappedData() const{ return static_cast<std::byte*>(mappedData); }
		void map() = delete;
		void unmap() = delete;
		void flush(VkDeviceSize size) const = delete;
		void invalidate(VkDeviceSize size) const = delete;

		[[nodiscard]] VkDevice getDevice() const noexcept = delete;
		[[nodiscard]] VkDeviceAddress getBufferAddress() const = delete;
	};


	class PersistentTransferDoubleBuffer : public PersistentBase{
	protected:
		StagingBuffer stagingBuffer{};
		ExclusiveBuffer targetBuffer{};

	public:
		[[nodiscard]] const StagingBuffer& getStagingBuffer() const{ return stagingBuffer; }

		[[nodiscard]] const ExclusiveBuffer& getTargetBuffer() const{ return targetBuffer; }

		[[nodiscard]] auto memorySize() const noexcept{
			return stagingBuffer.memory.size();
		}

		[[nodiscard]] PersistentTransferDoubleBuffer() = default;

		[[nodiscard]] PersistentTransferDoubleBuffer(
			VkPhysicalDevice physicalDevice, VkDevice device,
			VkDeviceSize size,
			VkBufferUsageFlags usage

			) :
			stagingBuffer{
				physicalDevice, device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
			}, targetBuffer{
				physicalDevice, device, size,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			}{

			map();
		}

		void map(){
			if(mappedData){
				throw std::runtime_error("Data Already Mapped");
			}
			mappedData = stagingBuffer.memory.map_noInvalidation();
		}

		void unmap(){
			if(!mappedData)return;
			stagingBuffer.memory.unmap();
			mappedData = nullptr;
		}

		void flush(VkDeviceSize size) const{
			stagingBuffer.memory.flush(size);
		}

		//TODO support
		void invalidate(VkDeviceSize size) const = delete;

		void cmdFlushToDevice(VkCommandBuffer commandBuffer, VkDeviceSize size = VK_WHOLE_SIZE) const{
			stagingBuffer.copyBuffer(commandBuffer, targetBuffer.get(), size);
		}

		[[nodiscard]] VkDevice getDevice() const noexcept{
			return targetBuffer.getDevice();
		}

		[[nodiscard]] VkDeviceAddress getBufferAddress() const{
			return targetBuffer.getBufferAddress();
		}
	};

	class PersistentTransferBuffer : public PersistentBase, public ExclusiveBuffer{

	public:
		[[nodiscard]] PersistentTransferBuffer() = default;

		[[nodiscard]] PersistentTransferBuffer(
			VkPhysicalDevice physicalDevice, VkDevice device,
			const VkDeviceSize size, const VkBufferUsageFlags usage) :
			ExclusiveBuffer{
				physicalDevice, device, size,
				usage,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			}{
			// map();
		}

		void map(){
			if(mappedData){
				throw std::runtime_error("Data Already Mapped");
			}
			mappedData = memory.map_noInvalidation();
		}

		void unmap(){
			if(!mappedData)return;
			memory.unmap();
			mappedData = nullptr;
		}

		void flush(VkDeviceSize size) const{
			memory.flush(size);
		}

		using ExclusiveBuffer::getDevice;

		[[nodiscard]] VkDeviceAddress getBufferAddress() const{
			return ExclusiveBuffer::getBufferAddress();
		}
	};
}

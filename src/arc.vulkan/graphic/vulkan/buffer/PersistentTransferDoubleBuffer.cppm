module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.PersistentTransferBuffer;

import Core.Vulkan.Buffer.ExclusiveBuffer;
import std;

export namespace Core::Vulkan{
	class PersistentTransferDoubleBuffer{
		StagingBuffer stagingBuffer{};
		ExclusiveBuffer targetBuffer{};

		void* mappedData{};

	public:
		[[nodiscard]] const StagingBuffer& getStagingBuffer() const{ return stagingBuffer; }

		[[nodiscard]] const ExclusiveBuffer& getTargetBuffer() const{ return targetBuffer; }

		[[nodiscard]] void* getMappedData() const{ return mappedData; }

		[[nodiscard]] auto size() const noexcept{
			return stagingBuffer.memory.size();
		}

		[[nodiscard]] PersistentTransferDoubleBuffer() = default;

		[[nodiscard]] PersistentTransferDoubleBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage) :
			targetBuffer{
				physicalDevice, device, size,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			}, stagingBuffer{
				physicalDevice, device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
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

		void flush(VkDeviceSize size){
			stagingBuffer.memory.flush(size);
		}

		//TODO support
		void invalidate(VkDeviceSize size) = delete;

		void cmdFlushToDevice(VkCommandBuffer commandBuffer, VkDeviceSize size = VK_WHOLE_SIZE) const{
			stagingBuffer.copyBuffer(commandBuffer, targetBuffer.get(), size);
		}
	};

	class PersistentTransferBuffer : public ExclusiveBuffer{
	protected:
		void* mappedData{};

	public:
		[[nodiscard]] std::byte* getMappedData() const{ return static_cast<std::byte*>(mappedData); }

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
			if(!mappedData)return; //TODO throw?
			memory.unmap();
			mappedData = nullptr;
		}

		void flush(VkDeviceSize size) const{
			memory.flush(size);
		}

		template <typename T = void>
		[[nodiscard]] T* getData() const noexcept{
			return static_cast<T*>(mappedData);
		}

		//TODO support
		void invalidate(VkDeviceSize size) = delete;
	};
}

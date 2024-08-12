module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.PersistentTransferBuffer;

import Core.Vulkan.Buffer.ExclusiveBuffer;
import std;

export namespace Core::Vulkan{
	class PersistentTransferBuffer{
		StagingBuffer stagingBuffer{};
		ExclusiveBuffer targetBuffer{};

		void* mappedData{};

		bool waitingCopy{};

	public:
		[[nodiscard]] const StagingBuffer& getStagingBuffer() const{ return stagingBuffer; }

		[[nodiscard]] const ExclusiveBuffer& getTargetBuffer() const{ return targetBuffer; }

		[[nodiscard]] void* getMappedData() const{ return mappedData; }

		[[nodiscard]] bool isWaitingCopy() const{ return waitingCopy; }

		[[nodiscard]] auto size() const noexcept{
			return stagingBuffer.memory.size();
		}

		[[nodiscard]] PersistentTransferBuffer() = default;

		[[nodiscard]] PersistentTransferBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage) :
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
			waitingCopy = true;
		}

		//TODO support
		void invalidate(VkDeviceSize size) = delete;

		void cmdFlushToDevice(VkCommandBuffer commandBuffer, VkDeviceSize size = VK_WHOLE_SIZE) const{
			if(!waitingCopy){
				throw std::runtime_error("No Data Valid!");
			}

			stagingBuffer.copyBuffer(commandBuffer, targetBuffer.get(), size);
		}

		void reset(){
			waitingCopy = false;
		}
	};
}

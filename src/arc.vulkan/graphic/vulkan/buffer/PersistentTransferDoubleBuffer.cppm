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
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
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

		void flush(VkDeviceSize size, VkDeviceSize offset = 0) const{
			stagingBuffer.memory.flush(size, offset);
		}

		//TODO support
		void invalidate(VkDeviceSize size) const = delete;

		void cmdFlushToDevice(VkCommandBuffer commandBuffer, VkDeviceSize size = VK_WHOLE_SIZE) const{
			stagingBuffer.copyBuffer(commandBuffer, targetBuffer.get(), size);
		}

		void cmdFlushToDevice(VkCommandBuffer commandBuffer, const VkBufferCopy& copy) const{
			stagingBuffer.copyBuffer(commandBuffer, targetBuffer.get(), copy);
		}

		[[nodiscard]] VkDevice getDevice() const noexcept{
			return targetBuffer.getDevice();
		}

		[[nodiscard]] VkDeviceAddress getBufferAddress() const{
			return targetBuffer.getBufferAddress();
		}


		template <std::ranges::contiguous_range T>
			requires (std::ranges::sized_range<T>)
		void loadData(const T& range) const{
			const std::size_t dataSize = std::ranges::size(range) * sizeof(std::ranges::range_value_t<T>);

			this->loadData(std::ranges::data(range), dataSize, 0);
		}

		template <typename T>
			requires (std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
		void loadData(const T& data, const std::size_t offset = 0) const{
			static constexpr std::size_t dataSize = sizeof(T);

			this->loadData(&data, dataSize, offset);
		}

		template <typename T, std::size_t size>
		void loadData(const std::array<T, size>& data) const{
			this->loadData(data.data(), size, 0);
		}

		template <typename T, typename... Args>
		void emplace(Args&&... args) const{
			static constexpr std::size_t dataSize = sizeof(T);

			if(dataSize > stagingBuffer.memorySize()){
				throw std::runtime_error("Insufficient buffer capacity!");
			}

			new(mappedData) T(std::forward<Args>(args)...);
		}

		template <typename T>
			requires (std::is_trivially_copyable_v<T>)
		void loadData(const T* data) const{
			this->loadData(*data, 0);
		}


		void loadData(const void* src, const std::size_t size, const std::size_t offset = 0) const{
			if(size > stagingBuffer.memorySize() - offset){
				throw std::runtime_error("Insufficient buffer capacity!");
			}

			const auto pdata = mappedData;
			std::memcpy(static_cast<std::byte*>(pdata) + offset, src , size);
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

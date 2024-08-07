module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.ExclusiveBuffer;

import Core.Vulkan.LogicalDevice.Dependency;
import std;

export namespace Core::Vulkan{
	std::uint32_t findMemoryType(const std::uint32_t typeFilter, VkPhysicalDevice physicalDevice,
	                             VkMemoryPropertyFlags properties){
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for(std::uint32_t i = 0; i < memProperties.memoryTypeCount; i++){
			if(typeFilter & (1u << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties){

				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool, VkDevice device){
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool commandPool, VkDevice device,
	                           VkQueue queue){
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(queue, 1, &submitInfo, nullptr);
		vkQueueWaitIdle(queue);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}


	void copyBuffer(
		VkCommandPool commandPool, VkDevice device, VkQueue queue,
		VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size
	){
		VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool, device);

		VkBufferCopy copyRegion = {};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		endSingleTimeCommands(commandBuffer, commandPool, device, queue);
	}

	/**
	 * @brief owns the whole memory
	 */
	class ExclusiveBuffer{
		DeviceDependency device{};

		VkBuffer handler{};
		VkDeviceMemory memory{};

		std::size_t capacity{};

	public:
		[[nodiscard]] constexpr VkDevice getDevice() const noexcept{ return device; }

		[[nodiscard]] constexpr VkBuffer getHandler() const noexcept{ return handler; }

		[[nodiscard]] constexpr VkDeviceMemory getMemory() const noexcept{ return memory; }

		[[nodiscard]] constexpr std::size_t size() const noexcept{ return capacity; }

		[[nodiscard]] constexpr ExclusiveBuffer() = default;

		[[nodiscard]] explicit ExclusiveBuffer(VkDevice device)
			: device{device}{}

		[[nodiscard]] ExclusiveBuffer(VkDevice device, VkBuffer hander)
			: device{device},
			  handler{hander}{}

		template <std::invocable<VkDevice, ExclusiveBuffer&> InitFunc>
		[[nodiscard]] explicit ExclusiveBuffer(VkDevice device, InitFunc initFunc) : device{device}{
			initFunc(device, *this);
		}

		[[nodiscard]] ExclusiveBuffer(
			VkPhysicalDevice physicalDevice, VkDevice device,
			VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties
		) : device{device}, capacity{size}{
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

			if(vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS){
				throw std::runtime_error("Failed to allocate buffer memory!");
			}

			vkBindBufferMemory(device, handler, memory, 0);
		}

		~ExclusiveBuffer(){
			if(device){
				if(handler) vkDestroyBuffer(device, handler, nullptr);
				if(memory) vkFreeMemory(device, memory, nullptr);
				handler = nullptr;
				memory = nullptr;
				capacity = 0;
			}
		}

		template <std::ranges::contiguous_range T>
			requires (std::ranges::sized_range<T>)
		void loadData(const T& range) const{
			const std::size_t dataSize = std::ranges::size(range) * sizeof(std::ranges::range_value_t<T>);

			if(dataSize > capacity){
				throw std::runtime_error("Insufficient buffer capacity!");
			}

			const auto pdata = map();
			std::memcpy(pdata, std::ranges::data(range), dataSize);
			unmap();
		}

		template <typename T>
			requires (std::is_trivially_copyable_v<T>)
		void loadData(const T& data) const{
			static constexpr std::size_t dataSize = sizeof(T);

			if(dataSize > capacity){
				throw std::runtime_error("Insufficient buffer capacity!");
			}

			const auto pdata = map();
			std::memcpy(pdata, &data, dataSize);
			unmap();
		}

		template <typename T, typename... Args>
		void emplace(Args&&... args) const{
			static constexpr std::size_t dataSize = sizeof(T);

			if(dataSize > capacity){
				throw std::runtime_error("Insufficient buffer capacity!");
			}

			const auto pdata = map();
			new(pdata) T(std::forward<Args>(args)...);
			unmap();
		}

		template <typename T>
			requires (std::is_trivially_copyable_v<T>)
		void loadData(const T* data) const{
			this->loadData(*data);
		}

		[[nodiscard]] void* map() const{
			void* data{};
			vkMapMemory(device, memory, 0, capacity, 0, &data);
			return data;
		}

		void unmap() const{
			vkUnmapMemory(device, memory);
		}

		[[nodiscard]] constexpr operator VkBuffer() const noexcept{
			return handler;
		}

		ExclusiveBuffer(const ExclusiveBuffer& other) = delete;

		ExclusiveBuffer(ExclusiveBuffer&& other) noexcept = default;

		ExclusiveBuffer& operator=(const ExclusiveBuffer& other) = delete;

		ExclusiveBuffer& operator=(ExclusiveBuffer&& other) noexcept = default;
	};
}

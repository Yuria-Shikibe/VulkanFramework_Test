module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer;

import Geom.Vector2D;
import Graphic.Color;
import std;

export namespace Core::Vulkan{
	struct Vertex {
		Geom::Vec2 position{};
		Graphic::Color color{};
		Geom::Vec2 texCoord{};

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static auto getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[0].offset = std::bit_cast<std::uint32_t>(&Vertex::position);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attributeDescriptions[1].offset = std::bit_cast<std::uint32_t>(&Vertex::color);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = std::bit_cast<std::uint32_t>(&Vertex::texCoord);

			return attributeDescriptions;
		}
	};

	const std::vector<Vertex> test_vertices = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
		{{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
		{{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}}
	};

	std::uint32_t findMemoryType(const std::uint32_t typeFilter, VkPhysicalDevice physicalDevice, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (std::uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");

	}

	const std::vector<std::uint16_t> test_indices{0, 1, 2, 2, 3, 0};



	class DataBuffer{
		VkDevice device{};
		VkBuffer handler{};
		VkDeviceMemory memory{};

		std::size_t capacity{};

	public:
		[[nodiscard]] constexpr VkDevice getDevice() const noexcept{ return device; }

		[[nodiscard]] constexpr VkBuffer getHandler() const noexcept{ return handler; }

		[[nodiscard]] constexpr VkDeviceMemory getMemory() const noexcept{ return memory; }

		[[nodiscard]] constexpr std::size_t size() const noexcept{ return capacity; }

		[[nodiscard]] constexpr DataBuffer() = default;

		[[nodiscard]] explicit DataBuffer(VkDevice device)
			: device{device}{}

		[[nodiscard]] DataBuffer(VkDevice device, VkBuffer hander)
			: device{device},
			  handler{hander}{}

		template <std::invocable<VkDevice, DataBuffer&> InitFunc>
		[[nodiscard]] explicit DataBuffer(VkDevice device, InitFunc initFunc) : device{device}{
			initFunc(device, *this);
		}

		[[nodiscard]] DataBuffer(
			VkPhysicalDevice physicalDevice, VkDevice device,
			VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties
		) : device{device}, capacity{size} {
			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = usage;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (vkCreateBuffer(device, &bufferInfo, nullptr, &handler) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create buffer!");
			}

			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(device, handler, &memRequirements);

			VkMemoryAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, physicalDevice, properties);

			if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate buffer memory!");
			}

			vkBindBufferMemory(device, handler, memory, 0);
		}

		~DataBuffer(){
			free();
		}

		void free(){
			if(device){
				if(handler)vkDestroyBuffer(device, handler, nullptr);
				if(memory)vkFreeMemory(device, memory, nullptr);
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

			const auto pdata  = map();
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

			const auto pdata  = map();
			std::memcpy(pdata, &data, dataSize);
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

		DataBuffer(const DataBuffer& other) = delete;

		DataBuffer& operator=(const DataBuffer& other) = delete;

		DataBuffer(DataBuffer&& other) noexcept : device{other.device}, handler{other.handler}, memory{other.memory}, capacity{other.capacity} {
			other.handler = nullptr;
			other.memory = nullptr;
		}

		DataBuffer& operator=(DataBuffer&& other) noexcept{
			if(this == &other)return *this;
			device = other.device;
			handler = other.handler;
			memory = other.memory;
			capacity = other.capacity;

			other.handler = nullptr;
			other.memory = nullptr;

			return *this;
		}
	};

	VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool, VkDevice device) {
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

	void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool commandPool, VkDevice device, VkQueue queue) {
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
	) {
		VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool, device);

		VkBufferCopy copyRegion = {};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		endSingleTimeCommands(commandBuffer, commandPool, device, queue);
	}

	DataBuffer createIndexBuffer(VkPhysicalDevice physicalDevice, VkDevice device,
		VkCommandPool commandPool, VkQueue queue) {
		VkDeviceSize bufferSize = sizeof(decltype(test_indices)::value_type) * test_indices.size();

		DataBuffer stagingBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		stagingBuffer.loadData(test_indices);

		DataBuffer buffer(physicalDevice, device, bufferSize,
		                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		copyBuffer(commandPool, device, queue, stagingBuffer, buffer, bufferSize);

		return buffer;
	}

	[[nodiscard]] DataBuffer createVertexBuffer(
		VkPhysicalDevice physicalDevice, VkDevice device,
		VkCommandPool commandPool, VkQueue queue
		) {
		VkDeviceSize bufferSize = sizeof(decltype(test_vertices)::value_type) * test_vertices.size();

		DataBuffer stagingBuffer{
				physicalDevice, device, bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			};

		stagingBuffer.loadData(test_vertices);
		const auto dataPtr = stagingBuffer.map();
		std::memcpy(dataPtr, test_vertices.data(), stagingBuffer.size());
		stagingBuffer.unmap();

		DataBuffer buffer{
				physicalDevice, device, bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			};

		copyBuffer(commandPool, device, queue, stagingBuffer, buffer, bufferSize);

		return buffer;
	}

}

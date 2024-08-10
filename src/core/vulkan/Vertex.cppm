module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Vertex;

import Core.Vulkan.Buffer.ExclusiveBuffer;
import Core.Vulkan.Preinstall;
import Geom.Vector2D;
import Graphic.Color;
import std;
import ext.MetaProgramming;

export namespace Core::Vulkan{



	struct Vertex {
		Geom::Vec2 position{};
		Graphic::Color color{};
		Geom::Vec2 texCoord{};
	};

	using BindInfo = Util::VertexBindInfo<Vertex, 0,
		std::pair{&Vertex::position, VK_FORMAT_R32G32_SFLOAT},
		std::pair{&Vertex::color, VK_FORMAT_R32G32B32A32_SFLOAT},
		std::pair{&Vertex::texCoord, VK_FORMAT_R32G32_SFLOAT}
	>;

	const std::vector<Vertex> test_vertices = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.f}, {0.0f, 0.0f}},
		{{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.f}, {0.0f, 1.0f}},
		{{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f, .17f}, {1.0f, 1.0f}},
		{{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f, 1.f}, {1.0f, 0.0f}}
	};


	const std::vector<std::uint16_t> test_indices{0, 1, 2, 2, 3, 0};


	ExclusiveBuffer createIndexBuffer(const VkPhysicalDevice physicalDevice, const VkDevice device,
	                                  const VkCommandPool commandPool, const VkQueue queue) {
		VkDeviceSize bufferSize = sizeof(decltype(test_indices)::value_type) * test_indices.size();

		ExclusiveBuffer stagingBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		stagingBuffer.memory.loadData(test_indices);

		ExclusiveBuffer buffer(physicalDevice, device, bufferSize,
		                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		Util::copyBuffer(commandPool, device, queue, stagingBuffer, buffer, bufferSize);

		return buffer;
	}

	[[nodiscard]] ExclusiveBuffer createVertexBuffer(
		const VkPhysicalDevice physicalDevice, const VkDevice device,
		const VkCommandPool commandPool, const VkQueue queue
	) {
		VkDeviceSize bufferSize = sizeof(decltype(test_vertices)::value_type) * test_vertices.size();

		ExclusiveBuffer stagingBuffer{
				physicalDevice, device, bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			};

		stagingBuffer.memory.loadData(test_vertices);
		const auto dataPtr = stagingBuffer.memory.map();
		std::memcpy(dataPtr, test_vertices.data(), stagingBuffer.memory.size());
		stagingBuffer.memory.unmap();

		ExclusiveBuffer buffer{
				physicalDevice, device, bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			};

		Util::copyBuffer(commandPool, device, queue, stagingBuffer, buffer, bufferSize);

		return buffer;
	}

}

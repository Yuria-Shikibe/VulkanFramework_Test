module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Vertex;

import Core.Vulkan.Buffer.ExclusiveBuffer;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Context;
import Core.Vulkan.Preinstall;

import Geom.Vector2D;
import Graphic.Color;
import std;
import ext.MetaProgramming;

export namespace Core::Vulkan{

	struct Vertex {
		Geom::Vec2 position{};
		float depth{};
		Graphic::Color color{};
		Geom::Vec2 texCoord{};
	};

	struct InstanceDesignator{
		std::int8_t offset{};
	};

	using VertBindInfo = Util::VertexBindInfo<Vertex, 0, 0, VK_VERTEX_INPUT_RATE_VERTEX,
		std::pair{&Vertex::position, VK_FORMAT_R32G32B32_SFLOAT},
		// std::pair{&Vertex::depth, VK_FORMAT_R32_SFLOAT},
		std::pair{&Vertex::color, VK_FORMAT_R32G32B32A32_SFLOAT},
		std::pair{&Vertex::texCoord, VK_FORMAT_R32G32_SFLOAT}
	>;

	using InstanceBindInfo = Util::VertexBindInfo<InstanceDesignator, 0, VertBindInfo::size, VK_VERTEX_INPUT_RATE_INSTANCE,
		std::pair{&InstanceDesignator::offset, VK_FORMAT_R8_SINT}
	>;

	const std::vector<Vertex> test_vertices = {
		{{50 , 50 }, 0.f, {1.0f, 0.0f, 0.0f, 1.f}, {0.0f, 1.0f}},
		{{550, 50 }, 0.f, {0.0f, 1.0f, 0.0f, 1.f}, {1.0f, 1.0f}},
		{{550, 550}, 0.f, {0.0f, 0.0f, 1.0f, .17f}, {1.0f, 0.0f}},
		{{50 , 550}, 0.f, {1.0f, 1.0f, 1.0f, 1.f}, {0.0f, 0.0f}},

		{Geom::Vec2{-0.25f, -0.25f}.reverse().rotate(30.f), 0.f, {1.0f, 0.0f, 0.0f, 1.f}, {0.0f, 0.0f}},
		{Geom::Vec2{ 0.25f, -0.25f}.reverse().rotate(30.f), 0.f, {0.0f, 1.0f, 0.0f, 1.f}, {0.0f, 1.0f}},
		{Geom::Vec2{ 0.25f,  0.25f}.reverse().rotate(30.f), 0.f, {0.0f, 0.0f, 1.0f, .17f}, {1.0f, 1.0f}},
		{Geom::Vec2{-0.25f,  0.25f}.reverse().rotate(30.f), 0.f, {1.0f, 1.0f, 1.0f, 1.f}, {1.0f, 0.0f}}
	};


	[[nodiscard]] ExclusiveBuffer createVertexBuffer(const Context& context, VkCommandPool commandPool) {
		const VkDeviceSize bufferSize = sizeof(decltype(test_vertices)::value_type) * test_vertices.size();

		const StagingBuffer stagingBuffer{context.physicalDevice, context.device, bufferSize};

		stagingBuffer.memory.loadData(test_vertices);

		ExclusiveBuffer buffer{
				context.physicalDevice, context.device, bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			};

		stagingBuffer.copyBuffer(TransientCommand{context.device, commandPool, context.device.getGraphicsQueue()}, buffer);

		return buffer;
	}

}

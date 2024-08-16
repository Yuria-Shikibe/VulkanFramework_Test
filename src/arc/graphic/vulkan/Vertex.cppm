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

	struct BatchVertex {
		Geom::Vec2 position{};
		float depth{};
		std::uint32_t textureID{};
		Graphic::Color color{};
		Geom::Vec2 texCoord{};
	};

	struct InstanceDesignator{
		std::int8_t offset{};
	};

	using VertBindInfo = Util::VertexBindInfo<BatchVertex, 0, 0, VK_VERTEX_INPUT_RATE_VERTEX,
		std::pair{&BatchVertex::position, VK_FORMAT_R32G32B32_SFLOAT},
		std::pair{&BatchVertex::textureID, VK_FORMAT_R32_UINT},
		// std::pair{&Vertex::depth, VK_FORMAT_R32_SFLOAT},
		std::pair{&BatchVertex::texCoord, VK_FORMAT_R32G32_SFLOAT},
		std::pair{&BatchVertex::color, VK_FORMAT_R32G32B32A32_SFLOAT}
	>;

	using InstanceBindInfo = Util::VertexBindInfo<InstanceDesignator, 0, VertBindInfo::size, VK_VERTEX_INPUT_RATE_INSTANCE,
		std::pair{&InstanceDesignator::offset, VK_FORMAT_R8_SINT}
	>;

}

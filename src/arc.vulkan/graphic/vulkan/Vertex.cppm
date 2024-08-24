module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Vertex;

import Core.Vulkan.Preinstall;

import Geom.Vector2D;
import Graphic.Color;
import std;

export namespace Core::Vulkan{
	struct Vertex_World {
		Geom::Vec2 position{};
		float depth{};
		Util::TextureIndex textureParam{};
		Graphic::Color color{};
		Geom::Vec2 texCoord{};
	};

    struct InstanceDesignator{
        std::int8_t offset{};
    };

	using WorldVertBindInfo = Util::VertexBindInfo<Vertex_World, 0, 0, VK_VERTEX_INPUT_RATE_VERTEX,
		std::pair{&Vertex_World::position, VK_FORMAT_R32G32B32_SFLOAT},
		std::pair{&Vertex_World::textureParam, VK_FORMAT_R8G8B8A8_UINT},
		std::pair{&Vertex_World::texCoord, VK_FORMAT_R32G32_SFLOAT},
		std::pair{&Vertex_World::color, VK_FORMAT_R32G32B32A32_SFLOAT}
	>;

	using InstanceBindInfo = Util::VertexBindInfo<InstanceDesignator, 0, WorldVertBindInfo::size, VK_VERTEX_INPUT_RATE_INSTANCE,
		std::pair{&InstanceDesignator::offset, VK_FORMAT_R8_SINT}
	>;

}

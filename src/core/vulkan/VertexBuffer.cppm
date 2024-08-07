module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Vertex;

import Core.Vulkan.Buffer.ExclusiveBuffer;
import Geom.Vector2D;
import Graphic.Color;
import std;
import ext.MetaProgramming;

export namespace Core::Vulkan{

	/**
	 * @brief Only works for 4 Byte members!
	 * @tparam Vertex Target Vertex
	 * @tparam bindingIndex corresponding index
	 * @tparam attributes attribute list
	 */
	template <typename Vertex, std::uint32_t bindingIndex, auto ...attributes>
		requires requires{
			requires std::is_trivially_copy_assignable_v<Vertex>;
			// requires (std::same_as<Vertex, typename ext::GetMemberPtrInfo<typename decltype(attributes)::first_type>::ClassType> && ...);
			// requires (std::same_as<VkFormat, std::tuple_element_t<1, decltype(attributes)>> && ...);
		}
	struct VertexBindInfo{
	private:
		static constexpr std::uint32_t stride = sizeof(Vertex);
		static constexpr std::uint32_t binding = bindingIndex;

		static constexpr std::size_t attributeSize = sizeof...(attributes);

		using AttributeDesc = std::array<VkVertexInputAttributeDescription, attributeSize>;

		static constexpr auto getAttrInfo(){
			AttributeDesc bindings{};

			[&]<std::size_t... I>(std::index_sequence<I...>){
				(VertexBindInfo::bind(bindings[I], attributes, static_cast<std::uint32_t>(I)), ...);
			}(std::make_index_sequence<attributeSize>{});

			return bindings;
		}

		template <typename V>
		static constexpr void bind(VkVertexInputAttributeDescription& description, const std::pair<V Vertex::*, VkFormat>& attr, const std::uint32_t index){
			description.binding = binding;
			description.format = attr.second;
			description.location = index;
			description.offset = std::bit_cast<std::uint32_t, V Vertex::*>(attr.first); //...
		}

	public:
		static constexpr VkVertexInputBindingDescription bindDesc{
			.binding = binding,
			.stride = stride,
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		};


		inline const static AttributeDesc attrDesc{getAttrInfo()};
	};

	struct Vertex {
		Geom::Vec2 position{};
		Graphic::Color color{};
		Geom::Vec2 texCoord{};
	};

	using BindInfo = VertexBindInfo<Vertex, 0,
		std::pair{&Vertex::position, VK_FORMAT_R32G32_SFLOAT},
		std::pair{&Vertex::color, VK_FORMAT_R32G32B32A32_SFLOAT},
		std::pair{&Vertex::texCoord, VK_FORMAT_R32G32_SFLOAT}
	>;

	const std::vector<Vertex> test_vertices = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
		{{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
		{{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}}
	};


	const std::vector<std::uint16_t> test_indices{0, 1, 2, 2, 3, 0};


	ExclusiveBuffer createIndexBuffer(const VkPhysicalDevice physicalDevice, const VkDevice device,
	                                  const VkCommandPool commandPool, const VkQueue queue) {
		VkDeviceSize bufferSize = sizeof(decltype(test_indices)::value_type) * test_indices.size();

		ExclusiveBuffer stagingBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		stagingBuffer.loadData(test_indices);

		ExclusiveBuffer buffer(physicalDevice, device, bufferSize,
		                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		copyBuffer(commandPool, device, queue, stagingBuffer, buffer, bufferSize);

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

		stagingBuffer.loadData(test_vertices);
		const auto dataPtr = stagingBuffer.map();
		std::memcpy(dataPtr, test_vertices.data(), stagingBuffer.size());
		stagingBuffer.unmap();

		ExclusiveBuffer buffer{
				physicalDevice, device, bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			};

		copyBuffer(commandPool, device, queue, stagingBuffer, buffer, bufferSize);

		return buffer;
	}

}

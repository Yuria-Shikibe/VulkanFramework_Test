module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Preinstall;

import std;
import Core.Vulkan.Concepts;
import ext.Concepts;

namespace Core::Vulkan{
	export namespace Blending{
		constexpr auto DefaultMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;

		constexpr VkPipelineColorBlendAttachmentState Overwrite{
				.blendEnable = false,
				.colorWriteMask = DefaultMask
			};

		constexpr VkPipelineColorBlendAttachmentState Disable = Overwrite;

		constexpr VkPipelineColorBlendAttachmentState Discard{.colorWriteMask = 0};

		constexpr VkPipelineColorBlendAttachmentState ScaledAlphaBlend{
				.blendEnable = true,

				.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
				.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
				.colorBlendOp = VK_BLEND_OP_ADD,
				.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
				.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
				.alphaBlendOp = VK_BLEND_OP_ADD,

				.colorWriteMask = DefaultMask
			};

		constexpr VkPipelineColorBlendAttachmentState AlphaBlend{
				.blendEnable = true,

				.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
				.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
				.colorBlendOp = VK_BLEND_OP_ADD,
				.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
				.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		        .alphaBlendOp = VK_BLEND_OP_ADD,

				.colorWriteMask = DefaultMask
			};
	}

	export namespace Seq{
		template <VkDeviceSize ...offset>
		constexpr VkDeviceSize Offset[sizeof...(offset)]{offset...};

		constexpr auto NoOffset = Offset<0>;

		template <unsigned ...mask>
		constexpr unsigned StageFlagBits[sizeof...(mask)]{mask...};
	}

	export namespace Default{
		// template <VkDeviceSize offset = 0>
		// constexpr VkDeviceSize Offset[1]{offset};

		constexpr VkImageCreateInfo ImageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.extent = {0, 0, 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = 0,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};

		template <VkImageAspectFlags aspectMask>
		constexpr VkImageViewCreateInfo ImageViewCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.image = nullptr,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.components = {},
			.subresourceRange = {
				.aspectMask = aspectMask,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		constexpr VkImageViewCreateInfo ImageViewCreateInfo_Color = ImageViewCreateInfo<VK_IMAGE_ASPECT_COLOR_BIT>;

		constexpr VkImageViewCreateInfo ImageViewCreateInfo_Depth{[]() constexpr {
			auto info = ImageViewCreateInfo<VK_IMAGE_ASPECT_DEPTH_BIT>;

			info.format = VK_FORMAT_D32_SFLOAT;

			return info;
		}()};

		constexpr VkImageViewCreateInfo ImageViewCreateInfo_Stencil{[]() constexpr {
			auto info = ImageViewCreateInfo<VK_IMAGE_ASPECT_STENCIL_BIT>;

			info.format = VK_FORMAT_S8_UINT;

			return info;
		}()};

		constexpr VkImageViewCreateInfo ImageViewCreateInfo_DepthStencil{[]() constexpr {
			auto info = ImageViewCreateInfo<VK_IMAGE_ASPECT_STENCIL_BIT>;

			info.format = VK_FORMAT_D24_UNORM_S8_UINT;

			return info;
		}()};

		constexpr VkPipelineInputAssemblyStateCreateInfo InputAssembly{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
				.primitiveRestartEnable = false
			};

		constexpr VkPipelineDepthStencilStateCreateInfo DefaultDepthStencilState{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.depthTestEnable = true,
				.depthWriteEnable = true,
				.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
				.depthBoundsTestEnable = false,
				.stencilTestEnable = false,
				.front = {},
				.back = {},
				.minDepthBounds = 0.f,
				.maxDepthBounds = 1.f
			};

		template <std::uint32_t ViewportSize = 1>
		constexpr VkPipelineViewportStateCreateInfo DynamicViewportState{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.viewportCount = ViewportSize,
				.pViewports = nullptr,
				.scissorCount = ViewportSize,
				.pScissors = nullptr
			};

		constexpr VkPipelineViewportStateCreateInfo staticViewportState(const std::uint32_t size, VkViewport* viewportData, VkRect2D* scissorData) noexcept{
			return VkPipelineViewportStateCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.viewportCount = size,
				.pViewports = viewportData,
				.scissorCount = size,
				.pScissors = scissorData
			};
		}

		constexpr VkPipelineRasterizationStateCreateInfo Rasterizer{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,

				.depthClampEnable = false,
				.rasterizerDiscardEnable = false,
				.polygonMode = VK_POLYGON_MODE_FILL,
				.cullMode = VK_CULL_MODE_NONE, //Never cull for 2D draw
				.frontFace = VK_FRONT_FACE_CLOCKWISE,

				.depthBiasEnable = false,
				.depthBiasConstantFactor = .0f,
				.depthBiasClamp = .0f,
				.depthBiasSlopeFactor = .0f,
				.lineWidth = 1.f
			};

		constexpr VkPipelineMultisampleStateCreateInfo Multisampling{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
				.sampleShadingEnable = false,
				.minSampleShading = 1.0f,
				.pSampleMask = nullptr,
				.alphaToCoverageEnable = false,
				.alphaToOneEnable = false
			};

		template <ContigiousRange<VkPipelineColorBlendAttachmentState> auto R>
		constexpr VkPipelineColorBlendStateCreateInfo ColorBlending{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.logicOpEnable = false,
				.logicOp = VK_LOGIC_OP_COPY,
				.attachmentCount = static_cast<std::uint32_t>(std::ranges::size(R)),
				.pAttachments = std::ranges::data(R),
				.blendConstants = {.0f, .0f, .0f, .0f}
			};
	}


	namespace Util{
		/**
		 * @brief Only works for 4 Byte members!
		 * @tparam Vertex Target Vertex
		 * @tparam bindingIndex corresponding index
		 * @tparam attributes attribute list
		 */
		export template <typename Vertex,
			std::uint32_t bindingIndex, std::uint32_t beginLocation,
			VkVertexInputRate inputRate,
			auto ... attributes>
			requires requires{
				requires std::is_trivially_copy_assignable_v<Vertex>;
				// requires (std::same_as<Vertex, typename ext::GetMemberPtrInfo<typename decltype(attributes)::first_type>::ClassType> && ...);
				// requires (std::same_as<VkFormat, std::tuple_element_t<1, decltype(attributes)>> && ...);
			}
		struct VertexBindInfo{
			static constexpr std::uint32_t stride = sizeof(Vertex);
			static constexpr std::uint32_t binding = bindingIndex;

			static constexpr std::uint32_t size = sizeof...(attributes);

			using AttributeDesc = std::array<VkVertexInputAttributeDescription, size>;

		private:
			static constexpr auto getAttrInfo(){
				AttributeDesc bindings{};

				[&]<std::size_t... I>(std::index_sequence<I...>){
					(VertexBindInfo::bind(bindings[I], attributes, static_cast<std::uint32_t>(I)), ...);
				}(std::make_index_sequence<size>{});

				return bindings;
			}

			template <typename V>
			static constexpr void bind(VkVertexInputAttributeDescription& description,
			                           const std::pair<V Vertex::*, VkFormat>& attr, const std::uint32_t index){
				description.binding = binding;
				description.format = attr.second;
				description.location = beginLocation + index;
				description.offset = std::bit_cast<std::uint32_t, V Vertex::*>(attr.first); //...
			}

		public:
			static constexpr VkVertexInputBindingDescription BindDesc{
					.binding = binding,
					.stride = stride,
					.inputRate = inputRate
				};

			static VkPipelineVertexInputStateCreateInfo createInfo() noexcept{
				return {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.vertexBindingDescriptionCount = 1,
						.pVertexBindingDescriptions = &BindDesc,
						.vertexAttributeDescriptionCount = AttrDesc.size(),
						.pVertexAttributeDescriptions = AttrDesc.data()
					};
			}

			inline const static AttributeDesc AttrDesc{getAttrInfo()};
		};

		export struct EmptyVertexBind{
			static constexpr VkVertexInputBindingDescription BindDesc{};

			static constexpr VkPipelineVertexInputStateCreateInfo createInfo() noexcept{
				return {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
			}

			static constexpr std::array<VkVertexInputAttributeDescription, 1> AttrDesc{};
		};

		export template <typename T>
		concept VertexInfo = requires{
			{ T::createInfo() } -> std::same_as<VkPipelineVertexInputStateCreateInfo>;
		};

		template <std::size_t bindingsSize, std::size_t attributeSize>
		struct TempInfo{
			std::array<VkVertexInputBindingDescription, bindingsSize> bindings{};
			std::array<VkVertexInputAttributeDescription, attributeSize> attributes{};
		};

		export template <VertexInfo Prov>
		VkPipelineVertexInputStateCreateInfo getVertexInfo(){
			return Prov::createInfo();
		}

		template <std::size_t I, typename Args, std::size_t bSize, std::size_t aSize>
		void bindTo(std::uint32_t binding, std::size_t& off, TempInfo<bSize, aSize>& info) {
			using CurProv = std::tuple_element_t<I, Args>;

			info.bindings[I] = CurProv::BindDesc;
			info.bindings[I].binding = binding + I;

			[&]<std::size_t... Idx>(std::index_sequence<Idx...>){
				((info.attributes[off + Idx] = CurProv::AttrDesc[Idx]), ...);
			}(std::make_index_sequence<CurProv::size>{});

			[&]<std::size_t... Idx>(std::index_sequence<Idx...>){
				((info.attributes[off + Idx].binding = binding + I), ...);
			}(std::make_index_sequence<CurProv::size>{});

			off += CurProv::size;
		};

		/**
		 * @brief
		 * @tparam Prov Vertex Info Provider Type
		 * @param binding binding index
		 * @return [VkPipelineVertexInputStateCreateInfo, data], data should not be discard before create pipeline
		 * @warning NRVO MUST happen in this function, be careful when doing any modification
		 */
		export template <VertexInfo... Prov>
		[[nodiscard]] auto getVertexGroupInfo(std::uint32_t binding = 0){
			static constexpr std::size_t bSize = sizeof...(Prov);
			static constexpr std::size_t aSize = (Prov::size + ... + 0);

			using Args = std::tuple<Prov...>;

			std::pair<VkPipelineVertexInputStateCreateInfo, TempInfo<bSize, aSize>> ret{};

			std::size_t offset{};

			[&]<std::size_t... I>(std::index_sequence<I...>) {
				((Util::bindTo<I, Args>(binding, offset, ret.second)), ...);
			}(std::index_sequence_for<Prov...>{});

			ret.first = VkPipelineVertexInputStateCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.vertexBindingDescriptionCount = ret.second.bindings.size(),
				.pVertexBindingDescriptions = ret.second.bindings.data(),
				.vertexAttributeDescriptionCount = ret.second.attributes.size(),
				.pVertexAttributeDescriptions = ret.second.attributes.data()
			};

			return ret;
		}

		export
		VkPipelineDynamicStateCreateInfo createDynamicState(ContigiousRange<VkDynamicState> auto& states){
			return {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.dynamicStateCount = static_cast<std::uint32_t>(std::ranges::size(states)),
					.pDynamicStates = std::ranges::data(states),
				};
		}
	}
}

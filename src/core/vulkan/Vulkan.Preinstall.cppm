module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Preinstall;

import std;

export namespace Core::Vulkan{
	namespace Blending{
		constexpr auto DefaultMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		constexpr VkPipelineColorBlendAttachmentState Overwrite{
			.blendEnable = false,
			.colorWriteMask = DefaultMask
		};

		constexpr VkPipelineColorBlendAttachmentState Disable = Overwrite;

		constexpr VkPipelineColorBlendAttachmentState Discard{.colorWriteMask = 0};

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

	namespace Default{
		constexpr VkPipelineInputAssemblyStateCreateInfo InputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = false
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

		constexpr VkPipelineColorBlendStateCreateInfo ColorBlending{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.logicOpEnable = false,
			.logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = 1,
			.pAttachments = &Blending::Overwrite,
			.blendConstants = {}
		};
	}

	namespace Util{

	}
}

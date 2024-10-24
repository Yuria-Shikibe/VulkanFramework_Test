module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.PipelineLayout;

import ext.handle_wrapper;
import Core.Vulkan.Concepts;
import std;

export namespace Core::Vulkan{
	struct ConstantLayout{
		std::vector<VkPushConstantRange> constants{};

		void push(const VkPushConstantRange& constantRange){
			constants.push_back(constantRange);
		}
	};

	struct PipelineLayout : public ext::wrapper<VkPipelineLayout>{
		ext::dependency<VkDevice> device{};

		[[nodiscard]] PipelineLayout() = default;

		PipelineLayout(const PipelineLayout& other) = delete;

		PipelineLayout(PipelineLayout&& other) noexcept = default;

		PipelineLayout& operator=(const PipelineLayout& other) = delete;

		PipelineLayout& operator=(PipelineLayout&& other) noexcept = default;

		~PipelineLayout(){
			if(device)vkDestroyPipelineLayout(device, handle, nullptr);
		}

		template<
			ContigiousRange<VkDescriptorSetLayout> R1,
			ContigiousRange<VkPushConstantRange> R2 = EmptyRange<VkPushConstantRange>>
		[[nodiscard]] PipelineLayout(VkDevice device, VkPipelineLayoutCreateFlags flags,
			R1&& descriptorSetLayouts, R2&& constantRange = {}) : device{device}{

			VkPipelineLayoutCreateInfo pipelineLayoutInfo{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
					.flags = flags,
					.setLayoutCount = static_cast<std::uint32_t>(std::ranges::size(descriptorSetLayouts)),
					.pSetLayouts = std::ranges::data(descriptorSetLayouts),
					.pushConstantRangeCount = static_cast<std::uint32_t>(std::ranges::size(constantRange)),
					.pPushConstantRanges = std::ranges::data(constantRange)
				};

			if(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create pipeline layout!");
			}
		}
	};
}

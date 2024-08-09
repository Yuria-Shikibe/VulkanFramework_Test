module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.PipelineLayout;

import Core.Vulkan.Dependency;
import Core.Vulkan.Concepts;
import std;

export namespace Core::Vulkan{
	struct PipelineLayout{
		Dependency<VkDevice> device{};
		VkPipelineLayout pipelineLayout{};

		[[nodiscard]] PipelineLayout() = default;


		PipelineLayout(const PipelineLayout& other) = delete;

		PipelineLayout(PipelineLayout&& other) noexcept = default;

		PipelineLayout& operator=(const PipelineLayout& other) = delete;

		PipelineLayout& operator=(PipelineLayout&& other) noexcept{
			if(this == &other) return *this;
			this->~PipelineLayout();
			device = std::move(other.device);
			pipelineLayout = other.pipelineLayout;
			return *this;
		}

		~PipelineLayout(){
			if(device)vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		}

		[[nodiscard]] constexpr operator VkPipelineLayout() const noexcept{return pipelineLayout;}

		template<ContigiousRange<VkDescriptorSetLayout> R1, ContigiousRange<VkPushConstantRange> R2 = EmptyRange<VkPushConstantRange>>
		[[nodiscard]] PipelineLayout(VkDevice device,
			R1&& descriptorSetLayouts, R2&& constantRange = {}) : device{device}{

			VkPipelineLayoutCreateInfo pipelineLayoutInfo{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
					.setLayoutCount = static_cast<std::uint32_t>(std::ranges::size(descriptorSetLayouts)),
					.pSetLayouts = std::ranges::data(descriptorSetLayouts),
					.pushConstantRangeCount = static_cast<std::uint32_t>(std::ranges::size(constantRange)),
					.pPushConstantRanges = std::ranges::data(constantRange)
				};

			if(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS){
				throw std::runtime_error("Failed to create pipeline layout!");
			}
		}
	};
}

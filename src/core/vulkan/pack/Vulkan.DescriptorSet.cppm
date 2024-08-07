module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.DescriptorSet;

import Core.Vulkan.LogicalDevice.Dependency;
import std;
import ext.RuntimeException;

export namespace Core::Vulkan{

	class DescriptorSetLayoutBuilder{
		struct VkDescriptorSetLayoutBinding_Mapper{
			constexpr bool operator()(const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs) const noexcept{
				return lhs.binding < rhs.binding;
			}
		};

		std::set<VkDescriptorSetLayoutBinding, VkDescriptorSetLayoutBinding_Mapper> bindings{};

	public:
		void push(const VkDescriptorSetLayoutBinding& binding){
			const auto [rst, success] = bindings.insert(binding);

			if(!success){
				throw ext::RuntimeException(std::format("Conflicted Desc Layout At[{}]", binding.binding));
			}
		}

		void push(const std::uint32_t index, const VkDescriptorType type, const VkShaderStageFlags stageFlags, const std::uint32_t count = 1){
			push({.binding = index, .descriptorType = type, .descriptorCount = count, .stageFlags = stageFlags, .pImmutableSamplers = nullptr});
		}

		void push_seq(const VkDescriptorType type, const VkShaderStageFlags stageFlags, const std::uint32_t count = 1){
			push(bindings.size(), type, stageFlags, count);
		}

		[[nodiscard]] std::vector<VkDescriptorSetLayoutBinding> exportBindings() const{
			return {bindings.begin(), bindings.end()};
		}
	};


	class DescriptorSetLayout{
		VkDescriptorSetLayout descriptorSetLayout{};
		DeviceDependency device{};

	public:
		operator VkDescriptorSetLayout() const noexcept{return descriptorSetLayout;}

		[[nodiscard]] const VkDescriptorSetLayout& get() const{ return descriptorSetLayout; }

		[[nodiscard]] DescriptorSetLayout() = default;

		~DescriptorSetLayout(){
			if(device)vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		}

		DescriptorSetLayout(const DescriptorSetLayout& other) = delete;

		DescriptorSetLayout(DescriptorSetLayout&& other) noexcept = default;

		DescriptorSetLayout& operator=(const DescriptorSetLayout& other) = delete;

		DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept = default;

		DescriptorSetLayout(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
			this->device = device;

			VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};

			layoutInfo.bindingCount = bindings.size();
			layoutInfo.pBindings = bindings.data();

			if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create descriptor set layout!");
			}
		}
	};
}


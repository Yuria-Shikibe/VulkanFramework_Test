module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Uniform;

export import Geom.Matrix3D;
import std;

export namespace Core::Vulkan{
	struct UniformBufferObject {
		Geom::Matrix3D mat3{};
		float v1{};
		float v2{};
		float v3{};
	};

	[[nodiscard]] VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device) {
		VkDescriptorSetLayoutBinding uboLayoutBinding = {};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;

		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

		VkDescriptorSetLayout descriptorSetLayout{};

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &uboLayoutBinding;

		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor set layout!");
		}

		return descriptorSetLayout;
	}



}
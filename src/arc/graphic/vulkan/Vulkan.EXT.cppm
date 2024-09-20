module;

#include <vulkan/vulkan.h>
#include "vk_loader.hpp"

export module Core.Vulkan.EXT;
import std;

namespace Core::Vulkan::EXT{
	export std::add_pointer_t<decltype(vkGetPhysicalDeviceProperties2KHR)> getPhysicalDeviceProperties2KHR = nullptr;
	export std::add_pointer_t<decltype(vkGetDescriptorSetLayoutSizeEXT)> getDescriptorSetLayoutSizeEXT = nullptr;
	export std::add_pointer_t<decltype(vkGetDescriptorSetLayoutBindingOffsetEXT)> getDescriptorSetLayoutBindingOffsetEXT = nullptr;
	export std::add_pointer_t<decltype(vkGetDescriptorEXT)> getDescriptorEXT = nullptr;
	export std::add_pointer_t<decltype(vkCmdBindDescriptorBuffersEXT)> cmdBindDescriptorBuffersEXT = nullptr;
	export std::add_pointer_t<decltype(vkCmdSetDescriptorBufferOffsetsEXT)> cmdSetDescriptorBufferOffsetsEXT = nullptr;

	export void load(VkInstance instance){
		getPhysicalDeviceProperties2KHR = LoadFuncPtr(instance, vkGetPhysicalDeviceProperties2KHR);
		getDescriptorSetLayoutSizeEXT = LoadFuncPtr(instance, vkGetDescriptorSetLayoutSizeEXT);
		getDescriptorSetLayoutBindingOffsetEXT = LoadFuncPtr(instance, vkGetDescriptorSetLayoutBindingOffsetEXT);
		getDescriptorEXT = LoadFuncPtr(instance, vkGetDescriptorEXT);
		cmdBindDescriptorBuffersEXT = LoadFuncPtr(instance, vkCmdBindDescriptorBuffersEXT);
		cmdSetDescriptorBufferOffsetsEXT = LoadFuncPtr(instance, vkCmdSetDescriptorBufferOffsetsEXT);
	}
}


module;

#include <vulkan/vulkan.h>
#include "../arc.vulkan/graphic/vulkan/vk_loader.hpp"

module Core.Vulkan.Validation;

import ext.stack_trace;

VkBool32 Core::Vulkan::validationCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData){

	std::string str{pCallbackData->pMessage};

	const auto back = std::ranges::find_last(str, '(').begin();
	std::ranges::replace(std::ranges::subrange{str.begin(), back}, ':', '\n');
	str.insert(back, '\n');

	std::println(std::cerr, "[Vulkan] ID:{} | {}", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName);

	std::println(std::cerr, "{}", str);
	ext::print_stack_trace(std::cerr);

	return false;
}

VkResult Core::Vulkan::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback){

	if(const auto* func = LoadFuncPtr(instance, vkCreateDebugUtilsMessengerEXT)){
		return func(instance, pCreateInfo, pAllocator, pCallback);
	} else{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void Core::Vulkan::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback,
	const VkAllocationCallbacks* pAllocator){
	if(const auto* func = LoadFuncPtr(instance, vkDestroyDebugUtilsMessengerEXT)){
		func(instance, callback, pAllocator);
	}
}

module;

#include <vulkan/vulkan.h>
#include "../vk_loader.hpp"

export module Core.Vulkan.Validation;

import std;
import StackTrace;
import Core.Vulkan.Util.Invoker;

namespace Core::Vulkan{
	VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData){
		std::println(std::cerr, "{}", pCallbackData->pMessage);
		ext::getStackTraceBrief(std::cerr);

		return VK_FALSE;
	}

	VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pCallback){
		if(const auto func = LoadFuncPtr(instance, vkCreateDebugUtilsMessengerEXT)){
			return func(instance, pCreateInfo, pAllocator, pCallback);
		} else{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void DestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT callback,
		const VkAllocationCallbacks* pAllocator
	){
		if(const auto func = LoadFuncPtr(instance, vkDestroyDebugUtilsMessengerEXT)){
			func(instance, callback, pAllocator);
		}
	}

	export constexpr bool EnableValidationLayers{DEBUG_CHECK};

	export
	class ValidationEntry{
		VkInstance instance{};
		VkDebugUtilsMessengerEXT callback{};

	public:
		[[nodiscard]] constexpr ValidationEntry() noexcept = default;

		[[nodiscard]] ValidationEntry(
			VkInstance instance,
			PFN_vkDebugUtilsMessengerCallbackEXT callbackFuncPtr = validationCallback,
			const VkDebugUtilsMessengerCreateFlagsEXT flags = 0,
			const VkDebugUtilsMessageSeverityFlagsEXT messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			const VkDebugUtilsMessageTypeFlagsEXT messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) : instance(instance){

			if constexpr (EnableValidationLayers){
				const VkDebugUtilsMessengerCreateInfoEXT createInfo{
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
					.flags = flags,
					.messageSeverity = messageSeverity,
					.messageType = messageType,
					.pfnUserCallback = callbackFuncPtr,
					.pUserData = nullptr
				};

				if(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &callback)){
					throw std::runtime_error("Failed to set up debug callback!");
				}

				std::println("[Vulkan] Validation Callback Setup Succeed: {:#X}", reinterpret_cast<std::uintptr_t>(callbackFuncPtr));
			}
		}

		~ValidationEntry(){
			if constexpr (EnableValidationLayers){
				if(callback && instance)DestroyDebugUtilsMessengerEXT(instance, callback, nullptr);
			}
		}

		ValidationEntry(const ValidationEntry& other) = delete;

		ValidationEntry(ValidationEntry&& other) noexcept
			: instance{other.instance},
			  callback{other.callback}{
			other.callback = nullptr;
		}

		ValidationEntry& operator=(const ValidationEntry& other) = delete;

		ValidationEntry& operator=(ValidationEntry&& other) noexcept{
			if(this == &other) return *this;
			instance = other.instance;
			callback = other.callback;
			other.callback = nullptr;
			return *this;
		}
	};
}

module;

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

export module Core.Vulkan.Instance;

import Core.Vulkan.Util;
import Core.Vulkan.Validation;

import std;

namespace Core::Vulkan{
	/**
	 * @return All Required Extensions
	 */
	std::vector<const char*> getRequiredExtensions_GLFW(){
		std::uint32_t glfwExtensionCount{};
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		return {glfwExtensions, glfwExtensions + glfwExtensionCount};
	}

	std::vector<const char*> getRequiredExtensions(){
		auto extensions = getRequiredExtensions_GLFW();

		if constexpr(EnableValidationLayers){
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);


		return extensions;
	}

	constexpr VkApplicationInfo DefaultAppInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Hello Triangle",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "No Engine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_3,
	};


	[[nodiscard]] bool checkValidationLayerSupport(const decltype(UsedValidationLayers)& validationLayers = UsedValidationLayers){
		auto [availableLayers, rst] = Util::enumerate(vkEnumerateInstanceLayerProperties);

		return Util::checkSupport(validationLayers, availableLayers, &VkLayerProperties::layerName);
	}

	export
	class Instance{
		VkInstance instance{};

	public:
		[[nodiscard]] constexpr Instance() = default;

		void init(){
			if(instance != nullptr){
				throw std::runtime_error("Instance is already initialized");
			}

			VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
			createInfo.pApplicationInfo = &DefaultAppInfo;

			if constexpr(EnableValidationLayers){
				if(!checkValidationLayerSupport(UsedValidationLayers)){
					throw std::runtime_error("validation layers requested, but not available!");
				}

				std::println("Using Validation Layer: ");

				for(const auto& validationLayer : UsedValidationLayers){
					std::println("\t{}", validationLayer);
				}

				createInfo.enabledLayerCount = static_cast<std::uint32_t>(UsedValidationLayers.size());
				createInfo.ppEnabledLayerNames = UsedValidationLayers.data();
			} else{
				createInfo.enabledLayerCount = 0;
			}

			const auto extensions = getRequiredExtensions();
			createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
			createInfo.ppEnabledExtensionNames = extensions.data();

			if(vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS){
				throw std::runtime_error("failed to create vulkan instance!");
			}

			std::println("[Vulkan] Instance Create Succeed: {:#X}", reinterpret_cast<std::uintptr_t>(instance));
		}

		void free(){ //TODO is this necessary?
			vkDestroyInstance(instance, nullptr);
			instance = nullptr;
		}

		~Instance(){
			free();
		}

		Instance(const Instance& other) = delete;

		Instance(Instance&& other) noexcept
			: instance{other.instance}{
			other.instance = nullptr;
		}

		Instance& operator=(const Instance& other) = delete;

		Instance& operator=(Instance&& other) noexcept{
			if(this == &other) return *this;
			instance = other.instance;
			other.instance = nullptr;

			return *this;
		}

		[[nodiscard]] operator VkInstance() const noexcept{ return instance; }
	};
}
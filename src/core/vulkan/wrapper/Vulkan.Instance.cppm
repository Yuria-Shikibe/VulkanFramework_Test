module;

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

export module Core.Vulkan.Instance;

import Core.Vulkan.Util.Invoker;
import Core.Vulkan.Validation;

import std;
import ext.MetaProgramming;

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

		return extensions;
	}

	VkApplicationInfo DefaultAppInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Hello Triangle",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "No Engine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0),
	};

	export constexpr std::array UsedValidationLayers{
		"VK_LAYER_KHRONOS_validation"
	};

	[[nodiscard]] bool checkValidationLayerSupport(const decltype(UsedValidationLayers)& validationLayers = UsedValidationLayers){
		auto [availableLayers, rst] = Util::enumerate(vkEnumerateInstanceLayerProperties);

		std::unordered_set<std::string> requiredLayers(validationLayers.begin(), validationLayers.end());

		for(const auto& extension : availableLayers){
			requiredLayers.erase(extension.layerName);
		}

		return requiredLayers.empty();
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

			std::println("Init VK Instance Succeed: {:#X}", reinterpret_cast<std::uintptr_t>(instance));
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

		[[nodiscard]] constexpr operator const VkInstance&() const noexcept{ return instance; }
	};

	export
	struct PhysicalDevice{
		VkPhysicalDevice device{}; // = VK_NULL_HANDLE;

		explicit constexpr operator bool() const noexcept{
			return device != nullptr;
		}

		[[nodiscard]] std::string getName() const{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(device, &deviceProperties);

			return deviceProperties.deviceName;
		}

		[[nodiscard]] std::uint32_t rateDeviceSuitability() const{
			// Application can't function without geometry shaders
			//Minimum Requirements
			if(!meetFeatures(&VkPhysicalDeviceFeatures::geometryShader)){
				return 0;
			}

			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(device, &deviceProperties);

			std::uint32_t score = 0;

			// Discrete GPUs have a significant performance advantage
			if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
				score += 1000;
			}

			// Maximum possible size of textures affects graphics quality
			score += deviceProperties.limits.maxImageDimension2D;

			return score;
		}

		template <typename... Args>
			requires requires(VkPhysicalDeviceFeatures features, Args... args){
			requires (std::same_as<VkBool32, typename ext::GetMemberPtrInfo<Args>::ValueType> && ...);
			requires std::conjunction_v<std::is_member_object_pointer<Args>...>;
			}
		[[nodiscard]] bool meetFeatures(Args... args) const{
			VkPhysicalDeviceFeatures deviceFeatures;
			vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

			return (static_cast<bool>(deviceFeatures.*args) && ...);
		}

		[[nodiscard]] constexpr operator const VkPhysicalDevice&() const noexcept{
			return device;
		}
	};

	export
	class VkCore{
	public:
		Instance instance{};
		ValidationEntry validationEntry{};

		PhysicalDevice selectedDevice{};

		void init(){
			instance.init();
			if constexpr (EnableValidationLayers){
				validationEntry = ValidationEntry{instance};
			}
		}

		// void selectPhysicalDevice(){
		// 	auto [devices, rst] = Util::enumerate(vkEnumeratePhysicalDevices, VkInstance(instance));
		//
		// 	if(devices.empty()){
		// 		throw std::runtime_error("Failed to find GPUs with Vulkan support!");
		// 	}
		//
		// 	// Use an ordered map to automatically sort candidates by increasing score
		// 	std::multimap<std::uint32_t, PhysicalDevice, std::greater<std::uint32_t>> candidates;
		//
		// 	for(const auto& device : devices | std::ranges::views::transform([](auto& d){
		// 		return PhysicalDevice{d};
		// 	})){
		// 		candidates.emplace(device.rateDeviceSuitability(), device);
		// 	}
		//
		// 	// Check if the best candidate is suitable at all
		// 	for(const auto& device : candidates
		// 		| std::views::filter(
		// 			[this](const decltype(candidates)::value_type& item){
		// 				return item.first;// && isPhysicalDeviceValid(item.second.device);
		// 			})
		// 		| std::views::values){
		// 		selectedDevice = device;
		// 		break;
		// 		}
		//
		// 	if(!selectedDevice){
		// 		throw std::runtime_error("Failed to find a suitable GPU!");
		// 	} else{
		// 		std::println("[Vulkan] Selected Physical Device: {}", selectedDevice.getName());
		// 	}
		// }
	};
}
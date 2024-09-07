module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.PhysicalDevice;

import std;

import Core.Vulkan.Util;
import Core.Vulkan.SwapChainInfo;

import ext.MetaProgramming;


export namespace Core::Vulkan{
	constexpr std::array DeviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
		// VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
	};

	struct QueueFamilyIndices{
		static constexpr auto InvalidFamily = std::numeric_limits<std::uint32_t>::max();

		// 0 by default
		std::uint32_t graphicsFamily{};
		std::uint32_t presentFamily{};
		std::uint32_t computeFamily{InvalidFamily};

		[[nodiscard]] constexpr bool isComplete() const noexcept{
			return graphicsFamily != InvalidFamily && presentFamily != InvalidFamily && computeFamily != InvalidFamily;
		}

		[[nodiscard]] explicit operator bool() const noexcept{
			return isComplete();
		}

		[[nodiscard]] QueueFamilyIndices() = default;

		[[nodiscard]] QueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface) : graphicsFamily{InvalidFamily}, presentFamily{InvalidFamily}{
			std::vector<VkQueueFamilyProperties> queueFamilies = Util::enumerate(vkGetPhysicalDeviceQueueFamilyProperties, device);

			for(const auto& [index, queueFamily] : queueFamilies | std::ranges::views::enumerate){
				if(!queueFamily.queueCount) continue;

				if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT){
					graphicsFamily = index;
				}

				if(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT){
					computeFamily = index;
				}

				//Obtain present queue
				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &presentSupport);
				if(presentSupport){
					presentFamily = index;
				}

				if(isComplete()){
					break;
				}
			}
		}
	};

	struct PhysicalDevice{
		VkPhysicalDevice device{}; // = VK_NULL_HANDLE;

		QueueFamilyIndices queues{};
		VkPhysicalDeviceMemoryProperties deviceMemoryProperties{};

		[[nodiscard]] constexpr std::uint32_t findMemoryType(const std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const{
			for(std::uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++){
				if(typeFilter & (1u << i) && (deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties){
					return i;
				}
			}

			throw std::runtime_error("failed to find suitable memory type!");
		}

		[[nodiscard]] explicit operator bool() const noexcept{
			return device != nullptr;
		}

		[[nodiscard]] VkPhysicalDeviceProperties getPhysicalDeviceProperties() const{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(device, &deviceProperties);
			return deviceProperties;
		}

		[[nodiscard]] std::uint32_t rateDeviceSuitability() const{
			VkPhysicalDeviceProperties deviceProperties = getPhysicalDeviceProperties();

			std::uint32_t score = 0;

			// Discrete GPUs have a significant performance advantage
			if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
				score += 1000;
			}

			// Maximum possible size of textures affects graphics quality
			score += deviceProperties.limits.maxImageDimension2D;

			return score;
		}

		[[nodiscard]] bool checkDeviceExtensionSupport(const auto& requiredExtensions) const{
			auto [availableExtensions, rst] = Util::enumerate(vkEnumerateDeviceExtensionProperties, device, nullptr);

			return Util::checkSupport(requiredExtensions, availableExtensions, &VkExtensionProperties::extensionName);
		}

		[[nodiscard]] std::string getName() const{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(device, &deviceProperties);

			return std::string{deviceProperties.deviceName};
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

		[[nodiscard]] bool meetFeatures(const VkPhysicalDeviceFeatures& required) const noexcept{
			static constexpr std::size_t FeatureListSize = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);

			VkPhysicalDeviceFeatures deviceFeatures;
			vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

			auto& availableSrc = reinterpret_cast<const std::array<VkBool32, FeatureListSize>&>(deviceFeatures);
			auto& requiredSrc = reinterpret_cast<const std::array<VkBool32, FeatureListSize>&>(required);

			for(std::size_t i = 0; i < FeatureListSize; ++i){
				if(!availableSrc[i] && requiredSrc[i]) return false;
			}

			return true;
		}

		[[nodiscard]] explicit(false) constexpr operator VkPhysicalDevice() const noexcept{
			return device;
		}

		bool isPhysicalDeviceValid(VkSurfaceKHR surface = nullptr) const{
			const bool extensionsSupported = this->checkDeviceExtensionSupport(DeviceExtensions);

			const bool featuresMeet = meetFeatures(&VkPhysicalDeviceFeatures::samplerAnisotropy);

			if(!surface)return featuresMeet;

			const QueueFamilyIndices indices(device, surface);

			bool swapChainAdequate = false;
			if(extensionsSupported){
				const Core::Vulkan::SwapChainInfo swapChainSupport(device, surface);
				swapChainAdequate = swapChainSupport.isValid();
			}

			return indices.isComplete() && swapChainAdequate && featuresMeet;
		}

		void cacheProperties(VkSurfaceKHR surface){
			vkGetPhysicalDeviceMemoryProperties(device, &deviceMemoryProperties);
			queues = QueueFamilyIndices{device, surface};
		}
	};
}

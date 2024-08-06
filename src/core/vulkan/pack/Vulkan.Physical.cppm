module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Physical;

import std;
import Core.Vulkan.Util;
import ext.MetaProgramming;


export namespace Core::Vulkan{
	const std::vector<const char*> DeviceExtensions{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

	struct QueueFamilyIndices{
		static constexpr auto InvalidFamily = std::numeric_limits<std::uint32_t>::max();

		std::uint32_t graphicsFamily{};
		std::uint32_t presentFamily{};

		[[nodiscard]] constexpr bool isComplete() const noexcept{
			return graphicsFamily != InvalidFamily && presentFamily != InvalidFamily;
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

	struct SwapChainInfo{
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats{};
		std::vector<VkPresentModeKHR> presentModes{};

		[[nodiscard]] SwapChainInfo() = default;

		[[nodiscard]] SwapChainInfo(VkPhysicalDevice device, VkSurfaceKHR surface){
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);

			auto [formats, rst1] = Util::enumerate(vkGetPhysicalDeviceSurfaceFormatsKHR, device, surface);
			this->formats = std::move(formats);

			auto [presents, rst2] = Util::enumerate(vkGetPhysicalDeviceSurfacePresentModesKHR, device, surface);
			this->presentModes = std::move(presents);
		}

		[[nodiscard]] bool isValid() const noexcept{
			return !formats.empty() && !presentModes.empty();
		}
	};

	struct PhysicalDevice{
		VkPhysicalDevice device{}; // = VK_NULL_HANDLE;

		[[nodiscard]] explicit operator bool() const noexcept{
			return device != nullptr;
		}

		[[nodiscard]] std::uint32_t rateDeviceSuitability() const{
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

		[[nodiscard]] bool checkDeviceExtensionSupport(auto& requiredExtensions) const{
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

		[[nodiscard]] constexpr operator VkPhysicalDevice() const noexcept{
			return device;
		}

		bool isPhysicalDeviceValid(VkSurfaceKHR surface = nullptr) const{
			const bool extensionsSupported = checkDeviceExtensionSupport(DeviceExtensions);

			const bool featuresMeet = meetFeatures(&VkPhysicalDeviceFeatures::samplerAnisotropy);

			if(!surface)return featuresMeet;

			const QueueFamilyIndices indices(device, surface);

			bool swapChainAdequate = false;
			if(extensionsSupported){
				const SwapChainInfo swapChainSupport(device, surface);
				swapChainAdequate = swapChainSupport.isValid();
			}

			return indices.isComplete() && swapChainAdequate && featuresMeet;
		}
	};
}

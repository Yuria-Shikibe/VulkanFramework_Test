module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.SwapChainInfo;
import Core.Vulkan.Util.Invoker;
import std;

export namespace Core::Vulkan{
	struct SwapChainInfo{
		static constexpr VkFormat TargetFormat = VK_FORMAT_R8G8B8A8_UNORM;
		static constexpr VkColorSpaceKHR TargetColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

		static constexpr VkPresentModeKHR PresentMode_Ideal = VK_PRESENT_MODE_MAILBOX_KHR;
		static constexpr VkPresentModeKHR PresentMode_Alter = VK_PRESENT_MODE_IMMEDIATE_KHR;
		static constexpr VkPresentModeKHR PresentMode_Fallback = VK_PRESENT_MODE_FIFO_KHR;


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

		static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats){
			if(availableFormats.size() == 1 && availableFormats.front().format == VK_FORMAT_UNDEFINED){
				return {TargetFormat, TargetColorSpace};
			}

			for(const auto& availableFormat : availableFormats){
				if(availableFormat.format == TargetFormat && availableFormat.colorSpace == TargetColorSpace){
					return availableFormat;
				}
			}

			return availableFormats.front();
		}

		static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes){
			VkPresentModeKHR fallBack = PresentMode_Fallback;

			for(const auto& availablePresentMode : availablePresentModes){
				if(availablePresentMode == PresentMode_Ideal){
					return availablePresentMode;
				}

				if(availablePresentMode == PresentMode_Alter){
					fallBack = availablePresentMode;
				}
			}

			return fallBack;
		}
	};
}

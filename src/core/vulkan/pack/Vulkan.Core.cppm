module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Core;

export import Core.Vulkan.Instance;
import Core.Vulkan.Validation;

import Core.Vulkan.Util.Invoker;
import Core.Vulkan.Util;
export import Core.Vulkan.Physical;

import ext.MetaProgramming;

import std;


export namespace Core::Vulkan{
	class VkCore{
	public:
		Instance instance{};
		ValidationEntry validationEntry{};

		PhysicalDevice selectedDevice{};

		void init(){
			instance.init();
			if constexpr(EnableValidationLayers){
				validationEntry = ValidationEntry{instance};
			}
		}

		void selectPhysicalDevice(VkSurfaceKHR surface){
			auto [devices, rst] = Util::enumerate(vkEnumeratePhysicalDevices, static_cast<VkInstance>(instance));

			if(devices.empty()){
				throw std::runtime_error("Failed to find GPUs with Vulkan support!");
			}

			// Use an ordered map to automatically sort candidates by increasing score
			std::multimap<std::uint32_t, PhysicalDevice, std::greater<std::uint32_t>> candidates;

			for(const auto& device : devices | std::ranges::views::transform([](auto& d){
				return PhysicalDevice{d};
			})){
				candidates.insert(std::make_pair(device.rateDeviceSuitability(), device));
			}

			// Check if the best candidate is suitable at all
			for(const auto& device : candidates
				| std::views::filter(
					[&](const decltype(candidates)::value_type& item){
						return item.first && item.second.isPhysicalDeviceValid(surface);
					})
				| std::views::values){
				selectedDevice = device;
				break;
			}

			if(!selectedDevice){
				throw std::runtime_error("Failed to find a suitable GPU!");
			} else{
				std::println("[Vulkan] Selected Physical Device: {}", selectedDevice.getName());
			}
		}
	};
}


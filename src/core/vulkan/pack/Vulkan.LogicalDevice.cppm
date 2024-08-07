module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.LogicalDevice;

export import Core.Vulkan.LogicalDevice.Dependency;
import Core.Vulkan.Validation;
import Core.Vulkan.PhysicalDevice;
import std;

export namespace Core::Vulkan{
	class LogicalDevice{
		DeviceDependency device{};
		VkQueue graphicsQueue{};
		VkQueue presentQueue{};

	public:
		[[nodiscard]] LogicalDevice() = default;

		~LogicalDevice(){
			vkDestroyDevice(device, nullptr);
		}

		[[nodiscard]] operator VkDevice() const noexcept{ return device; }

		[[nodiscard]] VkDevice getDevice() const noexcept{ return device; }

		[[nodiscard]] VkQueue getGraphicsQueue() const noexcept{ return graphicsQueue; }

		[[nodiscard]] VkQueue getPresentQueue() const noexcept{ return presentQueue; }

		LogicalDevice(const LogicalDevice& other) = delete;

		LogicalDevice(LogicalDevice&& other) noexcept = default;

		LogicalDevice& operator=(const LogicalDevice& other) = delete;

		LogicalDevice& operator=(LogicalDevice&& other) noexcept{
			if(this == &other) return *this;
			this->~LogicalDevice();
			device = std::move(other.device);
			graphicsQueue = other.graphicsQueue;
			presentQueue = other.presentQueue;
			return *this;
		}


		LogicalDevice(VkPhysicalDevice physicalDevice, const QueueFamilyIndices& indices){
			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
			const std::unordered_set uniqueQueueFamilies{indices.graphicsFamily, indices.presentFamily};

			constexpr float queuePriority = 1.0f;
			for(const std::uint32_t queueFamily : uniqueQueueFamilies){
				VkDeviceQueueCreateInfo queueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
				queueCreateInfo.queueFamilyIndex = queueFamily;
				queueCreateInfo.queueCount = 1;
				queueCreateInfo.pQueuePriorities = &queuePriority;
				queueCreateInfos.push_back(queueCreateInfo);
			}

			VkPhysicalDeviceFeatures deviceFeatures{};
			deviceFeatures.samplerAnisotropy = true;

			VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
			createInfo.pEnabledFeatures = &deviceFeatures;

			createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
			createInfo.pQueueCreateInfos = queueCreateInfos.data();

			createInfo.enabledExtensionCount = static_cast<std::uint32_t>(DeviceExtensions.size());
			createInfo.ppEnabledExtensionNames = DeviceExtensions.data();

			if constexpr(EnableValidationLayers){
				createInfo.enabledLayerCount = static_cast<std::uint32_t>(UsedValidationLayers.size());
				createInfo.ppEnabledLayerNames = UsedValidationLayers.data();
			} else{
				createInfo.enabledLayerCount = 0;
			}

			if(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device.handler) != VK_SUCCESS){
				throw std::runtime_error("Failed to create logical device!");
			}

			vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
			vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
		}
	};
}

module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Core;

export import Core.Vulkan.Instance;
export import Core.Vulkan.PhysicalDevice;
export import Core.Vulkan.LogicalDevice;

import Core.Vulkan.Validation;
import Core.Vulkan.Util;


import ext.MetaProgramming;
import std;


export namespace Core::Vulkan{
	class Context{
	public:
		Instance instance{};
		ValidationEntry validationEntry{};

		PhysicalDevice physicalDevice{};
		LogicalDevice device{};

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
			for(const auto& [score, device] : candidates){
				if(score && device.isPhysicalDeviceValid(surface)){
					physicalDevice = device;
					break;
				}
			}

			if(!physicalDevice){
				throw std::runtime_error("Failed to find a suitable GPU!");
			} else{
				std::println("[Vulkan] On Physical Device: {}", physicalDevice.getName());
			}

			physicalDevice.cacheProperties(surface);
		}

		VkResult commandSubmit_Graphics(VkSubmitInfo& submitInfo, VkFence fence = nullptr) const{
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			VkResult result = vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, fence);
			if(result) std::println(std::cerr, "[Vulkan] [{}] Failed to submit the command buffer!",
			                        static_cast<int>(result));
			return result;
		}

		VkResult commandSubmit_Graphics(VkCommandBuffer commandBuffer,
		                                VkSemaphore semaphore_imageIsAvailable = nullptr,
		                                VkSemaphore semaphore_renderingIsOver = nullptr,
		                                VkFence fence = nullptr,
		                                VkPipelineStageFlags waitDstStage_imageIsAvailable =
			                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) const{
			VkSubmitInfo submitInfo = {
					.commandBufferCount = 1,
					.pCommandBuffers = &commandBuffer
				};
			if(semaphore_imageIsAvailable)
				submitInfo.waitSemaphoreCount = 1,
					submitInfo.pWaitSemaphores = &semaphore_imageIsAvailable,
					submitInfo.pWaitDstStageMask = &waitDstStage_imageIsAvailable;
			if(semaphore_renderingIsOver)
				submitInfo.signalSemaphoreCount = 1,
					submitInfo.pSignalSemaphores = &semaphore_renderingIsOver;
			return commandSubmit_Graphics(submitInfo, fence);
		}

		VkResult commandSubmit_Graphics(VkCommandBuffer commandBuffer, VkFence fence = nullptr) const{
			VkSubmitInfo submitInfo{.commandBufferCount = 1,.pCommandBuffers = &commandBuffer};
			return commandSubmit_Graphics(submitInfo, fence);
		}

		VkResult commandSubmit_Compute(VkSubmitInfo& submitInfo, VkFence fence = nullptr) const{
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			VkResult result = vkQueueSubmit(device.getComputeQueue(), 1, &submitInfo, fence);
			if(result) std::println(std::cerr, "[Vulkan] [{}] Failed to submit the command buffer!", static_cast<int>(result));
			return result;
		}

		VkResult commandSubmit_Compute(VkCommandBuffer commandBuffer, VkFence fence = nullptr) const{
			VkSubmitInfo submitInfo{.commandBufferCount = 1,.pCommandBuffers = &commandBuffer};
			return commandSubmit_Compute(submitInfo, fence);
		}
	};
}

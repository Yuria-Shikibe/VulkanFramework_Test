module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Context;

export import Core.Vulkan.Instance;
export import Core.Vulkan.PhysicalDevice;
export import Core.Vulkan.LogicalDevice;

import Core.Vulkan.Validation;
import Core.Vulkan.Concepts;
import Core.Vulkan.Params;
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

		//TODO globalCommandPool?

		void init(){
			instance.init();
			if constexpr(EnableValidationLayers){
				validationEntry = ValidationEntry{instance};
			}
		}

		void createDevice(VkSurfaceKHR surface){
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
				std::println(std::cerr, "Failed to find a suitable GPU");
				throw std::runtime_error("Failed to find a suitable GPU!");
			} else{
				std::println("[Vulkan] On Physical Device: {}", physicalDevice.getName());
			}

			physicalDevice.cacheProperties(surface);

			device = LogicalDevice{physicalDevice, physicalDevice.queues};
		}

	    explicit(false) operator Param::Device() const {
		    return {device};
		}

	    explicit(false) operator Param::Device_WithPhysical() const {
		    return {device, physicalDevice};
		}

		VkResult commandSubmit_Graphics(const VkSubmitInfo& submitInfo, VkFence fence = nullptr) const{
			VkResult result = vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, fence);
			if(result)std::println(
				std::cerr, "[Vulkan] [{}] Failed to submit the command buffer!",
				static_cast<int>(result));
			return result;
		}

		VkResult commandSubmit_Graphics(VkCommandBuffer commandBuffer,
		                                VkSemaphore toWait = nullptr,
		                                VkSemaphore toSignal = nullptr,
		                                VkFence fence = nullptr,
		                                VkPipelineStageFlags waitDstStage_imageIsAvailable =
			                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) const{
			VkSubmitInfo submitInfo = {
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.commandBufferCount = 1,
					.pCommandBuffers = &commandBuffer
				};

			if(toWait){
				submitInfo.waitSemaphoreCount = 1;
				submitInfo.pWaitSemaphores = &toWait;
				submitInfo.pWaitDstStageMask = &waitDstStage_imageIsAvailable;
			}

			if(toSignal){
				submitInfo.signalSemaphoreCount = 1;
				submitInfo.pSignalSemaphores = &toSignal;
			}

			return commandSubmit_Graphics(submitInfo, fence);
		}

		VkResult commandSubmit_Graphics(VkCommandBuffer commandBuffer, VkFence fence = nullptr) const{
			VkSubmitInfo submitInfo{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.commandBufferCount = 1,
				.pCommandBuffers = &commandBuffer
			};
			return commandSubmit_Graphics(submitInfo, fence);
		}

		VkResult commandSubmit_Compute(const VkSubmitInfo& submitInfo, VkFence fence = nullptr) const{
			VkResult result = vkQueueSubmit(device.getComputeQueue(), 1, &submitInfo, fence);
			if(result) std::println(std::cerr, "[Vulkan] [{}] Failed to submit the command buffer!", static_cast<int>(result));
			return result;
		}

		VkResult commandSubmit_Compute(VkCommandBuffer commandBuffer, VkFence fence = nullptr) const{
			VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1,.pCommandBuffers = &commandBuffer};
			return commandSubmit_Compute(submitInfo, fence);
		}

		template <ContigiousRange<VkSemaphore> Rng = std::initializer_list<VkSemaphore>>
		void triggerSemaphore(VkCommandBuffer commandBuffer, const Rng& semaphores) const{
			VkSubmitInfo submitInfo = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.commandBufferCount = 1,
				.pCommandBuffers = &commandBuffer
			};

			submitInfo.signalSemaphoreCount = std::ranges::size(semaphores);
			submitInfo.pSignalSemaphores = std::ranges::data(semaphores);

			(void)commandSubmit_Graphics(submitInfo, nullptr);
			vkQueueWaitIdle(device.getGraphicsQueue());
		}
	};
}

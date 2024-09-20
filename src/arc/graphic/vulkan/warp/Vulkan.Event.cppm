module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Event;

import ext.handle_wrapper;

export namespace Core::Vulkan{
	struct Event : ext::wrapper<VkEvent>{
		ext::dependency<VkDevice> device{};

		~Event(){
			if(device)vkDestroyEvent(device, handle, nullptr);
		}

		[[nodiscard]] Event() = default;

		[[nodiscard]] explicit Event(VkDevice device, const bool isDeviceOnly = true) : device(device){
			const VkEventCreateInfo info{
				.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
				.pNext = nullptr,
				.flags = isDeviceOnly ? VK_EVENT_CREATE_DEVICE_ONLY_BIT : 0u
			};
			vkCreateEvent(device, &info, nullptr, &handle);
		}

		void set() const{
			vkSetEvent(device, handle);
		}

		void cmdSet(VkCommandBuffer commandBuffer, const VkDependencyInfo& dependencyInfo) const{
			vkCmdSetEvent2(commandBuffer, handle, &dependencyInfo);
		}

		void cmdReset(VkCommandBuffer commandBuffer, const VkPipelineStageFlags2 flags) const{
			vkCmdResetEvent2(commandBuffer, handle, flags);
		}

		void cmdWait(VkCommandBuffer commandBuffer, const VkDependencyInfo& dependencyInfo) const{
			vkCmdWaitEvents2(commandBuffer, 1, &handle, &dependencyInfo);
		}

		void reset() const{
			vkResetEvent(device, handle);
		}

		[[nodiscard]] VkResult getStatus() const{
			return vkGetEventStatus(device, handle);
		}

		[[nodiscard]] bool isSet() const{
			return vkGetEventStatus(device, handle) == VK_EVENT_SET;
		}

		Event(const Event& other) = delete;

		Event(Event&& other) noexcept = default;

		Event& operator=(const Event& other) = delete;

		Event& operator=(Event&& other) noexcept = default;
	};
}

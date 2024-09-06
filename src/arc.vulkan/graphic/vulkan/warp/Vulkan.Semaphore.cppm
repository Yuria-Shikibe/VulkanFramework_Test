module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Semaphore;

import ext.handle_wrapper;
import std;

export namespace Core::Vulkan{
	class Semaphore : public ext::wrapper<VkSemaphore>{
		ext::dependency<VkDevice> device{};

	public:
		[[nodiscard]] Semaphore() = default;

		[[nodiscard]] explicit Semaphore(VkDevice device) : device{device}{
			VkSemaphoreCreateInfo semaphoreInfo{
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0
				};


			if(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create Semaphore!");
			}
		}


		Semaphore(const Semaphore& other) = delete;

		Semaphore(Semaphore&& other) noexcept = default;

		Semaphore& operator=(const Semaphore& other) = delete;

		Semaphore& operator=(Semaphore&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroySemaphore(device, handle, nullptr);
			wrapper::operator =(std::move(other));
			device = std::move(other.device);
			return *this;
		}

		~Semaphore(){
			if(device)vkDestroySemaphore(device, handle, nullptr);
		}

		[[nodiscard]] VkSemaphoreSubmitInfo getSubmitInfo(const VkPipelineStageFlags2 flags) const{
			return {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.pNext = nullptr,
				.semaphore = handle,
				.value = 0,
				.stageMask = flags,
				.deviceIndex = 0
			};
		}

		void signal() const{
			const VkSemaphoreSignalInfo signalInfo{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
				.pNext = nullptr,
				.semaphore = handle,
				.value = 0
			};
			vkSignalSemaphore(device, &signalInfo);
		}
	};
}


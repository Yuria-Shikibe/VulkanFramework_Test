module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Semaphore;

import Core.Vulkan.Dependency;
import std;

export namespace Core::Vulkan{
	class Semaphore : public Wrapper<VkSemaphore>{
		Dependency<VkDevice> device{};

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

		void reset() const{

		}

		void wait(const std::uint32_t timeout = std::numeric_limits<std::uint64_t>::max()) const{

		}

		Semaphore(const Semaphore& other) = delete;

		Semaphore(Semaphore&& other) noexcept
			: Wrapper{other.handle},
			  device{std::move(other.device)}{}

		Semaphore& operator=(const Semaphore& other) = delete;

		Semaphore& operator=(Semaphore&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroySemaphore(device, handle, nullptr);
			Wrapper::operator =(std::move(other));
			device = std::move(other.device);
			return *this;
		}

		~Semaphore(){
			if(device)vkDestroySemaphore(device, handle, nullptr);
		}
	};
}


module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Fence;

import Core.Vulkan.Dependency;
import std;

export namespace Core::Vulkan{
	class Fence : public Wrapper<VkFence>{
		Dependency<VkDevice> device{};

	public:
		enum struct CreateFlags : VkFenceCreateFlags{
			noSignal = 0,
			signal = VK_FENCE_CREATE_SIGNALED_BIT,
		};

		[[nodiscard]] Fence() = default;

		[[nodiscard]] explicit Fence(VkDevice device, const CreateFlags flags) : device{device}{
			VkFenceCreateInfo fenceInfo{
				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.pNext = nullptr,
				.flags = VkFenceCreateFlags{std::to_underlying(flags)}
			};

			if(vkCreateFence(device, &fenceInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create fence!");
			}
		}

		void reset() const{
			vkResetFences(device, 1, &handle);
		}

		void wait(const std::uint64_t timeout = std::numeric_limits<std::uint64_t>::max()) const{
			vkWaitForFences(device, 1, &handle, true, timeout);
		}

		void waitAndReset(const std::uint64_t timeout = std::numeric_limits<std::uint64_t>::max()) const{
			wait(timeout);
			reset();
		}

		Fence(const Fence& other) = delete;

		Fence(Fence&& other) noexcept = default;

		Fence& operator=(const Fence& other) = delete;


		Fence& operator=(Fence&& other) noexcept = default;

		~Fence(){
			if(device)vkDestroyFence(device, handle, nullptr);
		}
	};
}

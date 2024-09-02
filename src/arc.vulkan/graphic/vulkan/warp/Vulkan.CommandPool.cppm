module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.CommandPool;

import Core.Vulkan.Dependency;
import Core.Vulkan.Buffer.CommandBuffer;
import std;

export namespace Core::Vulkan{


	struct CommandPool : Wrapper<VkCommandPool>{
		DeviceDependency device{};

		[[nodiscard]] CommandPool() = default;

		~CommandPool(){
			if(device)vkDestroyCommandPool(device, handle, nullptr);
		}

		CommandPool(const CommandPool& other) = delete;

		CommandPool(CommandPool&& other) noexcept = default;

		CommandPool& operator=(const CommandPool& other) = delete;

		CommandPool& operator=(CommandPool&& other) = default;

		CommandPool(VkDevice device, const std::uint32_t queueFamilyIndex, const VkCommandPoolCreateFlags flags) : device{device}{
			VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
			poolInfo.queueFamilyIndex = queueFamilyIndex;
			poolInfo.flags = flags; // Optional

			if(vkCreateCommandPool(device, &poolInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create command pool!");
			}
		}

		[[nodiscard]] CommandBuffer obtain(const VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const{
			return {device, handle, level};
		}

		[[nodiscard]] TransientCommand obtainTransient(VkQueue targetQueue) const{
			return TransientCommand{device, handle, targetQueue};
		}

		void resetAll(const VkCommandPoolResetFlags resetFlags) const{
			vkResetCommandPool(device, handle, resetFlags);
		}

		[[nodiscard]] std::vector<CommandBuffer> obtainArray(const std::size_t count, const VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const{
			std::vector<CommandBuffer> arr{};
			arr.reserve(count);

			for(std::size_t i = 0; i < count; ++i){
				arr.push_back(obtain(level));
			}

			return arr;
		}
	};

	struct MultiThreadedCommandPool{
		std::mutex mutex{};
		std::unordered_map<std::thread::id, CommandPool> commandPools{};

		VkDevice device{};
		std::uint32_t queue{};

		[[nodiscard]] MultiThreadedCommandPool() = default;

		[[nodiscard]] MultiThreadedCommandPool(VkDevice device, std::uint32_t queue)
			: device{device},
			  queue{queue}{}

		MultiThreadedCommandPool(const MultiThreadedCommandPool& other) = delete;

		MultiThreadedCommandPool(MultiThreadedCommandPool&& other) noexcept
			:
			  commandPools{std::move(other.commandPools)},
			  device{other.device},
			  queue{other.queue}{}

		MultiThreadedCommandPool& operator=(const MultiThreadedCommandPool& other) = delete;

		MultiThreadedCommandPool& operator=(MultiThreadedCommandPool&& other) noexcept{
			if(this == &other) return *this;
			commandPools = std::move(other.commandPools);
			device = other.device;
			queue = other.queue;
			return *this;
		}

		[[nodiscard]] const CommandPool& obtain(const VkCommandPoolCreateFlags flags, const std::thread::id id = std::this_thread::get_id()){
			std::lock_guard lk{mutex};
			return (commandPools[id] = CommandPool{device, queue, flags});
		}
	};
}

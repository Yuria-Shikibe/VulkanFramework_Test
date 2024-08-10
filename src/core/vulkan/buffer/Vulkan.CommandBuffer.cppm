module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.CommandBuffer;

import Core.Vulkan.Dependency;
import std;

export namespace Core::Vulkan{
	class CommandBuffer : public Wrapper<VkCommandBuffer>{
	protected:
		Dependency<VkDevice> device{};
		Dependency<VkCommandPool> pool{};
	
	public:
		[[nodiscard]] CommandBuffer() = default;

		CommandBuffer(VkDevice device, VkCommandPool commandPool,
			const VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) : device{device}, pool{commandPool}
		{
			VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
			allocInfo.level = level;

			allocInfo.commandPool = commandPool;
			allocInfo.commandBufferCount = 1;

			if(vkAllocateCommandBuffers(device, &allocInfo, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to allocate command buffers!");
			}
		}

		operator VkCommandBuffer() const noexcept{ return handle; };

		[[nodiscard]] const VkCommandBuffer& get() const noexcept { return handle; }

		~CommandBuffer(){
			if(device && pool)vkFreeCommandBuffers(device, pool, 1, &handle);
		}

		CommandBuffer(const CommandBuffer& other) = delete;

		CommandBuffer& operator=(const CommandBuffer& other) = delete;

		CommandBuffer(CommandBuffer&& other) noexcept = default;

		CommandBuffer& operator=(CommandBuffer&& other) noexcept{
			if(this == &other) return *this;
			if(device && pool)vkFreeCommandBuffers(device, pool, 1, &handle);
			Wrapper::operator=(std::move(other));
			device = std::move(other.device);
			pool = std::move(other.pool);
			return *this;
		}

		void begin(const VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, const VkCommandBufferInheritanceInfo* inheritance = nullptr) const{
			VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

			beginInfo.flags = flags;
			beginInfo.pInheritanceInfo = inheritance; // Optional

			if(vkBeginCommandBuffer(handle, &beginInfo) != VK_SUCCESS){
				throw std::runtime_error("Failed to begin recording command buffer!");
			}
		}

		void end() const{
			if(vkEndCommandBuffer(handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to record command buffer!");
			}
		}

		template <typename T, typename... Args>
			requires std::invocable<T, VkCommandBuffer, const Args&...>
		void push(T fn, const Args& ...args) const{
			std::invoke(fn, handle, args...);
		}

		void setViewport(const VkViewport& viewport) const{
			vkCmdSetViewport(handle, 0, 1, &viewport);
		}

		void setScissor(const VkRect2D& scissor) const{
			vkCmdSetScissor(handle, 0, 1, &scissor);
		}
	};

	struct [[jetbrains::guard]] ScopedCommand{
		CommandBuffer& handler;

		[[nodiscard]] explicit ScopedCommand(CommandBuffer& handler, const VkCommandBufferUsageFlags flags)
			: handler{handler}{
			handler.begin(flags);
		}

		~ScopedCommand() noexcept(false) {
			handler.end();
		}
	};

	struct [[jetbrains::guard]] TransientCommand : CommandBuffer{
		VkQueue targetQueue{};

		[[nodiscard]] TransientCommand() = default;

		[[nodiscard]] TransientCommand(VkDevice device, VkCommandPool commandPool,
			VkQueue targetQueue)
			: CommandBuffer{device, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY},
			  targetQueue{targetQueue}{
			static constexpr VkCommandBufferBeginInfo beginInfo{
					VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					nullptr,
					VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
				};

			vkBeginCommandBuffer(handle, &beginInfo);
		}

		~TransientCommand() noexcept(false) {
			vkEndCommandBuffer(handle);

			const VkSubmitInfo submitInfo{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.pNext = nullptr,
					.waitSemaphoreCount = 0,
					.pWaitSemaphores = nullptr,
					.pWaitDstStageMask = nullptr,
					.commandBufferCount = 1,
					.pCommandBuffers = &handle,
					.signalSemaphoreCount = 0,
					.pSignalSemaphores = nullptr
				};

			vkQueueSubmit(targetQueue, 1, &submitInfo, nullptr);
			vkQueueWaitIdle(targetQueue);
		}

		TransientCommand(const TransientCommand& other) = delete;

		TransientCommand(TransientCommand&& other) noexcept = delete;

		TransientCommand& operator=(const TransientCommand& other) = delete;

		TransientCommand& operator=(TransientCommand&& other) noexcept = delete;
	};
}
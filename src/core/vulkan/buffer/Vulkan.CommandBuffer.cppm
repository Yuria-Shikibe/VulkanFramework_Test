module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.CommandBuffer;

import Core.Vulkan.LogicalDevice.Dependency;
import std;

export namespace Core::Vulkan{
	class CommandBuffer{
		Dependency<VkDevice> device{};
		Dependency<VkCommandPool> pool{};

		VkCommandBuffer commandBuffer{};


	public:
		[[nodiscard]] CommandBuffer() = default;

		CommandBuffer(VkDevice device, VkCommandPool commandPool,
			const VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) : device{device}, pool{commandPool}
		{
			VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
			allocInfo.level = level;

			allocInfo.commandPool = commandPool;
			allocInfo.commandBufferCount = 1;

			if(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS){
				throw std::runtime_error("Failed to allocate command buffers!");
			}
		}

		operator VkCommandBuffer() const noexcept{ return commandBuffer; };

		[[nodiscard]] const VkCommandBuffer& get() const noexcept { return commandBuffer; }

		~CommandBuffer(){
			if(device && pool && commandBuffer)
				vkFreeCommandBuffers(
					device, pool, 1, &commandBuffer);
		}

		CommandBuffer(const CommandBuffer& other) = delete;

		CommandBuffer(CommandBuffer&& other) noexcept = default;

		CommandBuffer& operator=(const CommandBuffer& other) = delete;

		CommandBuffer& operator=(CommandBuffer&& other) noexcept = default;

		void begin(const VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, const VkCommandBufferInheritanceInfo* inheritance = nullptr) const{
			VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

			beginInfo.flags = flags;
			beginInfo.pInheritanceInfo = inheritance; // Optional

			if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS){
				throw std::runtime_error("Failed to begin recording command buffer!");
			}
		}

		void end() const{
			if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS){
				throw std::runtime_error("Failed to record command buffer!");
			}
		}

		template <typename T, typename... Args>
			requires std::invocable<T, VkCommandBuffer, const Args&...>
		void push(T fn, const Args& ...args) const{
			std::invoke(fn, commandBuffer, args...);
		}

		void setViewport(const VkViewport& viewport) const{
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		}

		void setScissor(const VkRect2D& scissor) const{
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		}
	};

	struct [[jetbrains::guard]] ScopedCommand{
		CommandBuffer& commandBuffer;

		[[nodiscard]] explicit ScopedCommand(CommandBuffer& commandBuffer)
			: commandBuffer{commandBuffer}{
			commandBuffer.begin();
		}

		~ScopedCommand() noexcept(false) {
			commandBuffer.end();
		}
	};
}
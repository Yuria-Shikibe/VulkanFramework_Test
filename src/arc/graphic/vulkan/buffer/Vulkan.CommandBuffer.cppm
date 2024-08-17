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

		[[nodiscard]] VkDevice getDevice() const noexcept{
			return device;
		}

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

		void begin(const VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, const VkCommandBufferInheritanceInfo& inheritance = {}) const{
			VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

			beginInfo.flags = flags;
			beginInfo.pInheritanceInfo = &inheritance; // Optional

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

		void blitDraw() const{
			vkCmdDraw(handle, 3, 1, 0, 0);
		}

		void reset(VkCommandBufferResetFlagBits flagBits = static_cast<VkCommandBufferResetFlagBits>(0)) const{
			vkResetCommandBuffer(handle, flagBits);
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

		[[nodiscard]] explicit ScopedCommand(CommandBuffer& handler,
			const VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			const VkCommandBufferInheritanceInfo& inheritance = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO})
			: handler{handler}{
			handler.begin(flags, inheritance);
		}

		~ScopedCommand() noexcept(false) {
			handler.end();
		}

		explicit(false) operator VkCommandBuffer() const noexcept{return handler.get();}

		CommandBuffer* operator->() const{
			return &handler;
		}
	};

	struct [[jetbrains::guard]] TransientCommand : CommandBuffer{
		static constexpr VkCommandBufferBeginInfo BeginInfo{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			nullptr,
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};

		VkQueue targetQueue{};

		std::vector<VkSemaphore> toWait{};
		std::vector<VkSemaphore> toSignal{};

		[[nodiscard]] TransientCommand() = default;

		[[nodiscard]] TransientCommand(CommandBuffer&& commandBuffer, VkQueue targetQueue) :
			CommandBuffer{
				std::move(commandBuffer)
			}, targetQueue{targetQueue}{
			vkBeginCommandBuffer(handle, &BeginInfo);
		}

		[[nodiscard]] TransientCommand(VkDevice device, VkCommandPool commandPool,
			VkQueue targetQueue)
			: CommandBuffer{device, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY},
			  targetQueue{targetQueue}{


			vkBeginCommandBuffer(handle, &BeginInfo);
		}

		~TransientCommand() noexcept(false) {
			submit();
		}

		TransientCommand(const TransientCommand& other) = delete;

		TransientCommand& operator=(const TransientCommand& other) = delete;

		TransientCommand(TransientCommand&& other) noexcept
			: CommandBuffer{std::move(other)},
			  targetQueue{other.targetQueue},
			  toWait{std::move(other.toWait)},
			  toSignal{std::move(other.toSignal)}{}

		TransientCommand& operator=(TransientCommand&& other) noexcept{
			if(this == &other) return *this;
			submit();
			CommandBuffer::operator =(std::move(other));
			targetQueue = other.targetQueue;
			toWait = std::move(other.toWait);
			toSignal = std::move(other.toSignal);
			return *this;
		}

	private:
		void submit(){
			if(!handle)return;

			vkEndCommandBuffer(handle);

			const VkSubmitInfo submitInfo{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.pNext = nullptr,
				.waitSemaphoreCount = static_cast<std::uint32_t>(toWait.size()),
				.pWaitSemaphores = toWait.data(),
				.pWaitDstStageMask = nullptr,
				.commandBufferCount = 1,
				.pCommandBuffers = &handle,
				.signalSemaphoreCount = 0,
				.pSignalSemaphores = nullptr
			};

			vkQueueSubmit(targetQueue, 1, &submitInfo, nullptr);
			vkQueueWaitIdle(targetQueue);
		}
	};
}
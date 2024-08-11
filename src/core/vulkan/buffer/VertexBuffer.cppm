module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.VertexBuffer;

export import Core.Vulkan.Buffer.ExclusiveBuffer;
export import Core.Vulkan.Buffer.CommandBuffer;
import std;

export namespace Core::Vulkan{
	struct VertexBuffer : ExclusiveBuffer{
		using ExclusiveBuffer::ExclusiveBuffer;
		//Reserved
	};

	template <typename T, std::size_t vertexGroupCount = 4> //TODO as field instead of template arg
	class DynamicVertexBuffer{
		std::uint32_t maxCount{};
		VertexBuffer vertexBuffer{};
		StagingBuffer vertexStagingBuffer{};

		ExclusiveBuffer indirectBuffer{};
		StagingBuffer indirectStagingBuffer{};

		void* mappedData{};
		void* indirectData{};

		std::atomic_uint32_t currentOffset{};
		bool waitingCopy{};

		void unmapIndirect(){
			if(!indirectData)return;
			indirectBuffer.memory.unmap();
			indirectData = nullptr;
		}

	public:

		static constexpr auto UnitOffset = vertexGroupCount * sizeof(T);

		[[nodiscard]] auto getMaxCount() const{ return maxCount; }

		[[nodiscard]] auto& getVertexBuffer() const noexcept{ return vertexBuffer; }

		[[nodiscard]] auto& getStagingBuffer() const noexcept{ return vertexStagingBuffer; }

		[[nodiscard]] auto& getIndirectCmdBuffer() const noexcept{ return indirectBuffer; }

		constexpr auto getMaximumOffset() const noexcept{
			return maxCount;
		}

		[[nodiscard]] DynamicVertexBuffer() = default;

		[[nodiscard]] DynamicVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, const std::uint32_t count) :
			maxCount{count},
			vertexBuffer{
				physicalDevice, device, count * UnitOffset,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			}, vertexStagingBuffer{
				physicalDevice, device, count * UnitOffset, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
			}, indirectBuffer{
				physicalDevice, device, sizeof(VkDrawIndexedIndirectCommand),
				VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			}, indirectStagingBuffer{
				physicalDevice, device, sizeof(VkDrawIndexedIndirectCommand),
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
			}{

		}

		[[nodiscard]] std::uint32_t getCount() const noexcept{
			return currentOffset.load();
		}

		void flush(const std::uint32_t indicesGroupCount = 6){
			auto count = currentOffset.load();
			if(count == 0) return;

			count = std::min(count, maxCount);

			vertexStagingBuffer.memory.flush(count * UnitOffset);
			waitingCopy = true;

			new (indirectData) VkDrawIndexedIndirectCommand{
					.indexCount = count * indicesGroupCount,
					.instanceCount = count,
					.firstIndex = 0,
					.vertexOffset = 0,
					.firstInstance = 0
				};
			indirectStagingBuffer.memory.flush();
		}

		void cmdFlushToDevice(const CommandBuffer& commandBuffer) const{
			const auto count = currentOffset.load();
			if(count == 0 || !waitingCopy)return;

			vertexStagingBuffer.copyBuffer(commandBuffer, vertexBuffer.get(), std::min(count, maxCount) * UnitOffset);
			indirectStagingBuffer.copyBuffer(commandBuffer, indirectBuffer.get());

		}

		void reset(){
			currentOffset.store(0);
			waitingCopy = false;
		}

		void map(){
			mappedData = vertexStagingBuffer.memory.map_noInvalidation();
			indirectData = indirectStagingBuffer.memory.map_noInvalidation();
		}

		void unmap(){
			if(!mappedData)return;
			vertexStagingBuffer.memory.unmap();
			mappedData = nullptr;
		}

		[[nodiscard]] constexpr bool any() const noexcept{
			return currentOffset.load();
		}

		[[nodiscard]] constexpr bool empty() const noexcept{
			return !any();
		}

		[[nodiscard]] constexpr bool isFull() const noexcept{
			return currentOffset >= maxCount;
		}

		[[nodiscard]] constexpr bool isValid() const noexcept{
			return !isFull() && !waitingCopy;
		}

		/**
		 * @tparam PT Vertex Group Type, or void default
		 * @return [mapped data ptr, is overflow]
		 */
		template <typename PT = std::array<T, vertexGroupCount>>
			requires (std::is_void_v<PT> || sizeof(PT) == UnitOffset)
		[[nodiscard("Mapped Pointer Should not be discard")]]
		std::pair<PT*, bool> acquireSegment() noexcept{
			const auto offset = currentOffset++;

			if constexpr(std::is_void_v<PT>){
				return {static_cast<std::uint8_t*>(mappedData) + offset * UnitOffset, offset >= maxCount};
			}else{
				return {static_cast<PT*>(mappedData) + offset, offset >= maxCount};

			}
		}

		DynamicVertexBuffer(const DynamicVertexBuffer& other) = delete;

		DynamicVertexBuffer& operator=(const DynamicVertexBuffer& other) = delete;

		DynamicVertexBuffer(DynamicVertexBuffer&& other) noexcept
			: maxCount{other.maxCount},
			  vertexBuffer{std::move(other.vertexBuffer)},
			  vertexStagingBuffer{std::move(other.vertexStagingBuffer)},
			  indirectBuffer{std::move(other.indirectBuffer)},
			  indirectStagingBuffer{std::move(other.indirectStagingBuffer)},
			  mappedData{other.mappedData},
			  currentOffset{other.currentOffset.load()},
			  waitingCopy{other.waitingCopy}{}

		DynamicVertexBuffer& operator=(DynamicVertexBuffer&& other) noexcept{
			if(this == &other) return *this;
			maxCount = other.maxCount;
			vertexBuffer = std::move(other.vertexBuffer);
			vertexStagingBuffer = std::move(other.vertexStagingBuffer);
			indirectBuffer = std::move(other.indirectBuffer);
			indirectStagingBuffer = std::move(other.indirectStagingBuffer);
			mappedData = other.mappedData;
			currentOffset = other.currentOffset.load();
			waitingCopy = other.waitingCopy;
			return *this;
		}
	};
}

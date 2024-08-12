module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.VertexBuffer;

export import Core.Vulkan.Buffer.ExclusiveBuffer;
export import Core.Vulkan.Buffer.CommandBuffer;
import std;
import Core.Vulkan.Buffer.PersistentTransferBuffer;

export namespace Core::Vulkan{


	template <typename T, std::size_t vertexGroupCount = 4> //TODO as field instead of template arg
	class DynamicVertexBuffer{
		std::uint32_t maxCount{};

		std::atomic_uint32_t currentOffset{};

	public:
		PersistentTransferBuffer vertexBuffer{};
		PersistentTransferBuffer indirectBuffer{};

		static constexpr auto UnitOffset = vertexGroupCount * sizeof(T);

		[[nodiscard]] auto getMaxCount() const{ return maxCount; }

		constexpr auto getMaximumOffset() const noexcept{
			return maxCount;
		}

		[[nodiscard]] DynamicVertexBuffer() = default;

		[[nodiscard]] DynamicVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, const std::uint32_t count) :
			maxCount{count},
			vertexBuffer{
				physicalDevice, device, count * UnitOffset, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			}, indirectBuffer{
				physicalDevice, device, sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
			}{}

		[[nodiscard]] std::uint32_t getCount() const noexcept{
			return currentOffset.load();
		}

		void flush(const std::uint32_t indicesGroupCount = 6){
			auto count = currentOffset.load();
			if(count == 0) return;

			count = std::min(count, maxCount);

			vertexBuffer.flush(count * UnitOffset);

			new (indirectBuffer.getMappedData()) VkDrawIndexedIndirectCommand{
					.indexCount = count * indicesGroupCount,
					.instanceCount = 1,
					.firstIndex = 0,
					.vertexOffset = 0,
					.firstInstance = 0
				};
			indirectBuffer.flush(sizeof(VkDrawIndexedIndirectCommand));
		}

		void cmdFlushToDevice(const CommandBuffer& commandBuffer) const{
			const auto count = currentOffset.load();
			if(count == 0)return;

			vertexBuffer.cmdFlushToDevice(commandBuffer, std::min(count, maxCount) * UnitOffset);
			indirectBuffer.cmdFlushToDevice(commandBuffer, sizeof(VkDrawIndexedIndirectCommand));

		}

		void reset(){
			currentOffset.store(0);
			vertexBuffer.reset();
			indirectBuffer.reset();
		}

		void map(){
			vertexBuffer.map();
			indirectBuffer.map();
		}

		void unmap(){
			vertexBuffer.unmap();
			indirectBuffer.unmap();
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
			return !isFull() && !vertexBuffer.isWaitingCopy();
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
				return {static_cast<std::uint8_t*>(vertexBuffer.getMappedData()) + offset * UnitOffset, offset >= maxCount};
			}else{
				return {static_cast<PT*>(vertexBuffer.getMappedData()) + offset, offset >= maxCount};
			}
		}

		DynamicVertexBuffer(const DynamicVertexBuffer& other) = delete;

		DynamicVertexBuffer& operator=(const DynamicVertexBuffer& other) = delete;

		DynamicVertexBuffer(DynamicVertexBuffer&& other) noexcept
			: maxCount{other.maxCount},
			  currentOffset{other.currentOffset.load()},
			  vertexBuffer{std::move(other.vertexBuffer)},
			  indirectBuffer{std::move(other.indirectBuffer)}{}

		DynamicVertexBuffer& operator=(DynamicVertexBuffer&& other) noexcept{
			if(this == &other) return *this;
			maxCount = other.maxCount;
			vertexBuffer = std::move(other.vertexBuffer);
			indirectBuffer = std::move(other.indirectBuffer);
			currentOffset = other.currentOffset.load();
			return *this;
		}
	};
}

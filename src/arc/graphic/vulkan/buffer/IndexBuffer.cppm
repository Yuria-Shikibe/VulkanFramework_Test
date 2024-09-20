module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.IndexBuffer;
import Core.Vulkan.Buffer.ExclusiveBuffer;
import Core.Vulkan.Context;
import Core.Vulkan.Buffer.CommandBuffer;
import std;

export namespace Core::Vulkan{
	struct IndexBuffer : ExclusiveBuffer{
		using ExclusiveBuffer::ExclusiveBuffer;

		VkIndexType indexType = VK_INDEX_TYPE_UINT32;

		[[nodiscard]] IndexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, const VkDeviceSize size,
		                          const VkIndexType indexType)
			: ExclusiveBuffer{
				  physicalDevice, device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			  }, indexType{indexType}{}
	};

	namespace Util{
		template <typename T>
		struct IndexType;

		template <>
		struct IndexType<std::uint32_t> : std::integral_constant<VkIndexType, VK_INDEX_TYPE_UINT32>{};

		template <>
		struct IndexType<std::uint16_t> : std::integral_constant<VkIndexType, VK_INDEX_TYPE_UINT16>{};

		template <std::size_t GroupCount, typename T = std::uint32_t, typename R, std::size_t size>
		constexpr auto generateIndexReferences(const std::array<R, size> Reference) -> std::array<T, GroupCount * size>{
			std::array<T, GroupCount * size> result{};

			const auto [min, max] = std::ranges::minmax(Reference);
			const auto data = std::ranges::data(Reference);
			const T stride = static_cast<T>(max - min + 1);

			//TODO overflow check

			for(std::size_t i = 0; i < GroupCount; ++i){
				for(std::size_t j = 0; j < size; ++j){
					result[j + i * size] = static_cast<T>(data[j]) + i * stride;
				}
			}

			return result;
		}

		constexpr std::array StandardIndexBase{0u, 1u, 2u, 2u, 3u, 0u};
		template <std::size_t size = 16'384>
		constexpr std::array BatchIndices = generateIndexReferences<size, std::uint32_t>(StandardIndexBase);

		constexpr std::uint32_t PrimitiveVertCount = StandardIndexBase.size();

		template <std::ranges::contiguous_range Rng>
		IndexBuffer createIndexBuffer(
			const Context& context, VkCommandPool commandPool, Rng&& range){
			const VkDeviceSize bufferSize = sizeof(std::ranges::range_value_t<Rng>) * std::ranges::size(range);

			IndexBuffer buffer(context.physicalDevice, context.device, bufferSize,
											   IndexType<std::ranges::range_value_t<Rng>>::value);
			{
				const StagingBuffer stagingBuffer(context.physicalDevice, context.device, bufferSize);

				stagingBuffer.memory.loadData(std::from_range, range);

				stagingBuffer.copyBuffer(TransientCommand{context.device, commandPool, context.device.getPrimaryGraphicsQueue()},
										 buffer);
			}

			return buffer;
		}
	}
}

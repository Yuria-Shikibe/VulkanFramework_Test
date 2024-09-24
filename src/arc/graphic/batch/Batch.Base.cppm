module;

#include <vulkan/vulkan.h>

export module Graphic.Batch.Base;

export import Core.Vulkan.Buffer.PersistentTransferBuffer;
export import Core.Vulkan.Buffer.IndexBuffer;
export import Core.Vulkan.Buffer.VertexBuffer;
export import Core.Vulkan.Buffer.ExclusiveBuffer;
export import Core.Vulkan.Buffer.CommandBuffer;
export import Core.Vulkan.CommandPool;
export import Core.Vulkan.Context;
export import Core.Vulkan.Fence;
export import Graphic.BatchData;
export import Core.Vulkan.Event;
export import Core.Vulkan.Util;

export import Core.Vulkan.DescriptorBuffer;
export import Core.Vulkan.DescriptorLayout;
export import Core.Vulkan.Preinstall;

import std;
import ext.array_queue;
import ext.circular_array;
import ext.cond_atomic;

namespace Graphic{
	template <std::size_t size = 2>
	static constexpr auto getBarriars(VkBuffer Vertex, VkBuffer Indirect, const Core::Vulkan::DescriptorBuffer_Double& db){
		std::array<VkBufferMemoryBarrier2, size> barriers{};
		barriers[0] = VkBufferMemoryBarrier2{
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
				.pNext = nullptr,

				.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
				.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,

				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.buffer = Indirect,
				.offset = 0,
				.size = VK_WHOLE_SIZE
			};

		barriers[1] = VkBufferMemoryBarrier2{
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
				.pNext = nullptr,
				.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
				.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.buffer = Vertex,
				.offset = 0,
				.size = VK_WHOLE_SIZE
			};

		return barriers;
	}

	export class VerticesDataBase;

	export
	template <std::derived_from<VerticesDataBase> VerticesData>
	struct BasicBatch;

	export
	constexpr std::size_t BatchMaxGroupCount{2048 * 4};

	export
	template <bool atomic>
	struct VerticesCounter{
		static constexpr std::uint32_t MaxCount = BatchMaxGroupCount;

	protected:
		ext::cond_atomic<std::uint32_t, atomic> count{};
		ext::cond_atomic<bool, atomic> acquirable{true};

	public:
		[[nodiscard]] VerticesCounter() = default;

		VerticesCounter(VerticesCounter&& other) requires (!atomic) = default;

		VerticesCounter& operator=(VerticesCounter&& other) requires (!atomic) = default;

		VerticesCounter(VerticesCounter&& other) noexcept requires (atomic){
			count = other.count.load();
			acquirable = other.acquirable.load();
		}

		VerticesCounter& operator=(VerticesCounter&& other) noexcept requires (atomic) {
			if(this == &other) return *this;
			count = other.count.load();
			acquirable = other.acquirable.load();
			return *this;
		}

		VerticesCounter(VerticesCounter&& other) noexcept = delete;

		VerticesCounter& operator=(VerticesCounter&& other) noexcept = delete;

		[[nodiscard]] bool isAcquirable() const noexcept{
			return acquirable;
		}

		void endAcquire() noexcept{
			acquirable = false;
		}

		[[nodiscard]] std::size_t getCount() const noexcept{
			return std::min(MaxCount, static_cast<std::uint32_t>(count));
		}

		[[nodiscard]] bool empty() const noexcept{
			return count == 0;
		}
	};

	export
	class VerticesDataBase/* : public VerticesCounter<atomic>*/{
	protected:
		Core::Vulkan::StagingBuffer buffer{};

		void* vertData{};

	public:
		Core::Vulkan::Event transferDoneEvent{};

		void setTransferring() const{
			transferDoneEvent.reset();
		}

		[[nodiscard]] bool isNotTransferring() const{
			return transferDoneEvent.isSet();
		}

		[[nodiscard]] VerticesDataBase() = default;

		template <std::derived_from<VerticesDataBase> VerticesData>
		[[nodiscard]] explicit VerticesDataBase(const BasicBatch<VerticesData>& batch);

		[[nodiscard]] Core::Vulkan::StagingBuffer& getBuffer() noexcept{
			return buffer;
		}

		VerticesDataBase(const VerticesDataBase& other) = delete;

		VerticesDataBase(VerticesDataBase&& other) noexcept = default;

		VerticesDataBase& operator=(const VerticesDataBase& other) = delete;

		VerticesDataBase& operator=(VerticesDataBase&& other) noexcept = default;
	};

	export
	template <bool atomic, bool requiresClean = true>
	struct UniversalVerticesData : public VerticesDataBase, public VerticesCounter<atomic>{

		void activate(const std::ptrdiff_t unitOffset) noexcept{
			if constexpr (requiresClean){
				std::memset(vertData, 0, unitOffset * std::min<std::uint32_t>(ext::adapted_atomic_exchange(this->count, 0), BatchMaxGroupCount));
			}
			this->acquirable = true;
		}

		/**
		 * @return [data ptr, success count]
		 */
		std::pair<std::byte*, std::size_t> acquire(const std::size_t count, const std::ptrdiff_t unitOffset) noexcept{
			const auto offset = this->count += count;

			std::size_t successCount{count};
			if(offset > BatchMaxGroupCount){
				successCount -= offset - BatchMaxGroupCount;
			}

			return {static_cast<std::byte*>(vertData) + (offset - count) * unitOffset, successCount};
		}
	};

	export
	struct BatchInterface{
		static constexpr std::size_t BufferSize{3};
		static constexpr std::size_t UnitSize{5};

		static constexpr std::size_t MaximumAllowedSamplersSize{4};

		static constexpr std::uint32_t VerticesGroupCount{4};
		static constexpr std::uint32_t IndicesGroupCount{6};

		static constexpr ImageIndex InvalidImageIndex{static_cast<ImageIndex>(~0U)};

		using ImageSet = std::array<VkImageView, MaximumAllowedSamplersSize>;

		struct CommandUnit{
			Core::Vulkan::ExclusiveBuffer vertexBuffer{};
			Core::Vulkan::PersistentTransferDoubleBuffer indirectBuffer{};
			Core::Vulkan::DescriptorBuffer_Double descriptorBuffer_Double{};

			Core::Vulkan::CommandBuffer transferCommand{};
			Core::Vulkan::Fence fence{};
			ImageSet usedImages_prev{};

			decltype(auto) getBarriers() const{
				return getBarriars(
						vertexBuffer,
						indirectBuffer.getTargetBuffer(),
						descriptorBuffer_Double);
			}

			auto getDescriptorBufferBindInfo() const{
				return descriptorBuffer_Double.getBindInfo(VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);
			}

			[[nodiscard]] CommandUnit() = default;

			[[nodiscard]] explicit CommandUnit(const BatchInterface& batch)
				:
				vertexBuffer{
					batch.context->physicalDevice, batch.context->device,
					(batch.unitOffset * BatchMaxGroupCount),
					VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
				},
				indirectBuffer{
					batch.context->physicalDevice, batch.context->device,
					sizeof(VkDrawIndexedIndirectCommand),
					VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
				},
				descriptorBuffer_Double{batch.context->physicalDevice, batch.context->device, batch.layout, batch.layout.size()},
				transferCommand{batch.context->device, batch.transientCommandPool},
				fence{batch.context->device, Core::Vulkan::Fence::CreateFlags::signal}
			{}

			void updateDescriptorSets(const ImageSet& imageSet, VkSampler sampler) const{
				const auto end = std::ranges::find(imageSet, static_cast<VkImageView>(nullptr));
				const std::span span{imageSet.begin(), end};

				const auto imageInfos =
					Core::Vulkan::Util::getDescriptorInfoRange_ShaderRead(sampler, span);

				descriptorBuffer_Double.loadImage(0, imageInfos);
			}
		};


		const Core::Vulkan::Context* context{};
		Core::Vulkan::CommandPool transientCommandPool{};

		//OPTM make unit offset as a template constexpr argument after the framework has basically done
		std::ptrdiff_t unitOffset{};

		Core::Vulkan::DescriptorLayout layout{};
		Core::Vulkan::IndexBuffer indexBuffer{};

		VkSampler sampler{};

		ext::circular_array<CommandUnit, UnitSize> units{};
		std::function<void(const CommandUnit&, std::size_t)> externalDrawCall{};

	protected:
		ImageSet currentUsedImages{};
		ImageIndex lastImageIndex{};
		VkImageView lastUsedImageView{};

	public:

		[[nodiscard]] BatchInterface() = default;

		[[nodiscard]] explicit BatchInterface(const Core::Vulkan::Context& context, const std::size_t vertexSize, VkSampler sampler) :
			context{&context},
			transientCommandPool{
				context.device,
				context.graphicFamily(),
				VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
			},
			unitOffset{static_cast<std::ptrdiff_t>(vertexSize * VerticesGroupCount)},
			layout{
				context.device, [](Core::Vulkan::DescriptorLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
											MaximumAllowedSamplersSize);

					layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
				}
			},
			indexBuffer{
				Core::Vulkan::Util::createIndexBuffer(
					context, transientCommandPool, Core::Vulkan::Util::BatchIndices<BatchMaxGroupCount>)
			}, sampler{sampler}{

			for(auto& commands : units)commands = CommandUnit{*this};
		}

		void setSampler(VkSampler sampler) noexcept{
			if(!sampler)throw std::invalid_argument("invalid null sampler");
			this->sampler = sampler;
		}

		void bindBuffersTo(VkCommandBuffer commandBuffer, const std::size_t unitIndex){
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, units[unitIndex].vertexBuffer.as_data(), Core::Vulkan::Seq::Offset<0>);
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, indexBuffer.indexType);
			vkCmdDrawIndexedIndirect(commandBuffer, units[unitIndex].indirectBuffer.getTargetBuffer(), 0, 1,
									 sizeof(VkDrawIndexedIndirectCommand));
		}

		[[nodiscard]] Core::Vulkan::TransientCommand obtainTransientCommand() const{
			return transientCommandPool.getTransient(context->device.getPrimaryGraphicsQueue());
		}

	protected:
		[[nodiscard]] ImageIndex getMappedImageIndex(VkImageView imageView){
			if(imageView == lastUsedImageView){
				return lastImageIndex;
			}

			for(const auto& [index, usedImage] : currentUsedImages | std::views::enumerate){
				if(usedImage == imageView){
					return index;
				}

				if(!usedImage){
					usedImage = lastUsedImageView = imageView;
					lastImageIndex = index;
					return index;
				}
			}

			return InvalidImageIndex;
		}

		constexpr void clearImageState() noexcept{
			currentUsedImages.fill(nullptr);
			lastImageIndex = 0;
			lastUsedImageView = nullptr;
		}
	};

	template <std::derived_from<VerticesDataBase> VerticesData>
	struct BasicBatch : BatchInterface{
		template <typename AppendType = void>
		struct DrawCommandSeq : std::conditional_t<
				std::is_void_v<AppendType>,
				std::array<Core::Vulkan::CommandBuffer, UnitSize>,
				std::array<std::pair<Core::Vulkan::CommandBuffer, AppendType>, UnitSize>>{};

	protected:
		struct SubmittedDrawCall{
			VerticesData* frameData{};
			ImageSet usedImages{};
		};

		std::array<VerticesData, BufferSize> frames{};
		std::size_t currentIndex{};
		ext::array_queue<SubmittedDrawCall, BufferSize> pendingDrawCalls{};

	public:
		[[nodiscard]] BasicBatch() = default;

		[[nodiscard]] explicit BasicBatch(const Core::Vulkan::Context& context, const std::size_t vertexSize, VkSampler sampler) :
					BatchInterface{context, vertexSize, sampler}{
			for(auto& frame : frames)static_cast<VerticesDataBase&>(frame) = {*this};
		}

	protected:
		VerticesData& currentVerticesData() noexcept{
			return frames[currentIndex];
		}

		void submitCommand(const CommandUnit& commandUnit, const std::size_t index) const{
			externalDrawCall(commandUnit, index);
		}

		void recordTransferCommand(const std::uint32_t transferCount, VerticesDataBase* frameData, CommandUnit& commandUnit) const{
			auto barriers = commandUnit.getBarriers();

			const VkDependencyInfo dependencyInfo{
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.bufferMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size()),
				.pBufferMemoryBarriers = barriers.data(),
			};

			new(commandUnit.indirectBuffer.getMappedData()) VkDrawIndexedIndirectCommand{
				.indexCount = transferCount * IndicesGroupCount,
				.instanceCount = 1,
				.firstIndex = 0,
				.vertexOffset = 0,
				.firstInstance = 0
			};

			commandUnit.fence.waitAndReset();

			{
				Core::Vulkan::ScopedCommand scopedCommand{
					commandUnit.transferCommand, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
				};

				if(transferCount){
					frameData->getBuffer().copyBuffer(
								  scopedCommand, commandUnit.vertexBuffer.get(), transferCount * unitOffset);
				}
				commandUnit.indirectBuffer.cmdFlushToDevice(scopedCommand, sizeof(VkDrawIndexedIndirectCommand));
				commandUnit.descriptorBuffer_Double.cmdFlushToDevice(scopedCommand, commandUnit.descriptorBuffer_Double.size);

				frameData->transferDoneEvent.cmdSet(scopedCommand, dependencyInfo);
			}
		}
	};

	template <std::derived_from<VerticesDataBase> VerticesData>
	VerticesDataBase::VerticesDataBase(const BasicBatch<VerticesData>& batch) :
		buffer{
			batch.context->physicalDevice,
			batch.context->device,
			(batch.unitOffset * BatchMaxGroupCount),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		},
		vertData{buffer.memory.map_noInvalidation()},
		transferDoneEvent{batch.context->device, false}{}
}

module : private;


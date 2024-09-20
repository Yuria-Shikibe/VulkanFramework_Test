module;

#include <vulkan/vulkan.h>

export module Graphic.Batch;

import Core.Vulkan.Buffer.PersistentTransferBuffer;
import Core.Vulkan.Buffer.IndexBuffer;
import Core.Vulkan.Buffer.VertexBuffer;
import Core.Vulkan.Buffer.ExclusiveBuffer;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.CommandPool;
import Core.Vulkan.Context;
import Core.Vulkan.Fence;
import Core.Vulkan.BatchData;
import Core.Vulkan.Event;
import Core.Vulkan.Util;

import Core.Vulkan.DescriptorBuffer;
import Core.Vulkan.DescriptorLayout;
import Core.Vulkan.Preinstall;

import std;
import ext.array_queue;
import ext.circular_array;

export namespace Graphic{

	//TODO support move assignment
	//OPTM use shared index buffer instead of exclusive one
	struct Batch{
		static constexpr std::size_t MaxGroupCount{2048 * 4};
		static constexpr std::size_t BufferSize{3};
		static constexpr std::size_t UnitSize{5};

		static constexpr std::size_t MaximumAllowedSamplersSize{4};

		static constexpr std::uint32_t VerticesGroupCount{4};
		static constexpr std::uint32_t IndicesGroupCount{6};

		const Core::Vulkan::Context* context{};
		Core::Vulkan::CommandPool transientCommandPool{};
		std::ptrdiff_t unitOffset{};
		using ImageSet = std::array<VkImageView, MaximumAllowedSamplersSize>;

		template <typename AppendType = void>
		struct DrawCommandSeq : std::conditional_t<
				std::is_void_v<AppendType>,
				std::array<Core::Vulkan::CommandBuffer, UnitSize>,
				std::array<std::pair<Core::Vulkan::CommandBuffer, AppendType>, UnitSize>>{};

		static constexpr ImageIndex InvalidImageIndex{static_cast<ImageIndex>(~0U)};

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

			[[nodiscard]] explicit CommandUnit(const Batch& batch)
				:
				vertexBuffer{
					batch.context->physicalDevice, batch.context->device,
					(batch.unitOffset * MaxGroupCount),
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

		class FrameData{
			static constexpr std::uint32_t MaxCount = MaxGroupCount;

			Core::Vulkan::StagingBuffer vertexStagingBuffer{};

			void* vertData{};

			std::atomic_uint32_t currentCount{};
			std::atomic_bool canAcquire{true};

			std::shared_mutex capturedMutex{};

		public:
			Core::Vulkan::Event transferEvent{};

			[[nodiscard]] FrameData() = default;

			[[nodiscard]] bool empty() const noexcept{
				return currentCount == 0;
			}

			void init(const Batch& batch){
				transferEvent = Core::Vulkan::Event{batch.context->device, false};
				vertexStagingBuffer = {
						batch.context->physicalDevice, batch.context->device, (batch.unitOffset * MaxGroupCount),
						VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					};
				vertData = vertexStagingBuffer.memory.map_noInvalidation();
			}

			[[nodiscard]] bool isAcquirable() const noexcept{
				return canAcquire;
			}

			[[nodiscard]] std::shared_lock<decltype(capturedMutex)> capture(){
				return std::shared_lock{capturedMutex};
			}

			[[nodiscard]] std::unique_lock<decltype(capturedMutex)> captureUnique(){
				return std::unique_lock{capturedMutex};
			}

			[[nodiscard]] std::size_t getCount() const noexcept{
				return std::min(MaxCount, currentCount.load());
			}

			void resetState(){
				transferEvent.reset();
			}

			void activate(const std::ptrdiff_t unitOffset) noexcept{
				std::memset(vertData, 0, unitOffset * std::min<unsigned>(currentCount.exchange(0), MaxGroupCount));
				canAcquire = true;
			}

			void close() noexcept{
				canAcquire = false;
			}

			[[nodiscard]] Core::Vulkan::StagingBuffer& getVertexStagingBuffer() noexcept{
				return vertexStagingBuffer;
			}

			/**
			 * @return [data ptr, success count]
			 */
			std::pair<std::byte*, std::size_t> acquire(const std::size_t count, const std::ptrdiff_t unitOffset) noexcept{
				const auto offset = currentCount += count;

				std::size_t successCount{count};
				if(offset > MaxCount){
					successCount -= offset - MaxCount;
				}

				return {static_cast<std::byte*>(vertData) + (offset - count) * unitOffset, successCount};
			}
		};

		struct DrawCall{
			FrameData* frameData{};
			ImageSet usedImages{};
		};

		Core::Vulkan::DescriptorLayout layout{};
		Core::Vulkan::IndexBuffer indexBuffer{};

		VkSampler sampler{};
		ImageSet usedImages{};

	private:
		std::shared_mutex imageUpdateMutex{};

		std::mutex overflowMutex{};

		std::array<FrameData, BufferSize> frames{};

		std::size_t currentIndex{};

		std::binary_semaphore semaphore_indexIncrement{1};

		/** @brief shared lock when acquire valid draw args, unique lock when swap frame*/
		std::shared_mutex frameSeekingMutex{};

		ext::array_queue<DrawCall, BufferSize> drawCalls{};
		std::mutex drawQueueMutex{};

	public:
		ext::circular_array<CommandUnit, UnitSize> units{};
		std::function<void(const CommandUnit&, std::size_t)> drawCall{};

		[[nodiscard]] Batch() = default;

		[[nodiscard]] explicit Batch(const Core::Vulkan::Context& context, const std::size_t vertexSize, VkSampler sampler) :
			context{&context}, transientCommandPool{
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
					context, transientCommandPool, Core::Vulkan::Util::BatchIndices<MaxGroupCount>)
			}, sampler{sampler}{

			for(auto& frame : frames)frame.init(*this);
			for(auto& commands : units)commands = CommandUnit{*this};
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

		void consumeAll(){

			while(true){
				if(!consumeFrame())break;
			}

			{
				std::shared_lock lkm{frameSeekingMutex};
				if(overflowMutex.try_lock()){
					std::lock_guard lk{overflowMutex, std::adopt_lock};
					if(!currentFrame().empty()){
						toNextFrame(currentIndex, lkm, true);
						consumeFrame();
					}
				}
			}
		}

		/**
		 * @brief multi call is allowed, do nothing when there is no frame valid
		 */
		bool consumeFrame(){
			DrawCall topDrawCall;

			{
				std::lock_guard lkq{drawQueueMutex};

				if(drawCalls.empty()){
					return false;
				}

				topDrawCall = drawCalls.front();
				drawCalls.pop();
			}

			auto index = units.get_index();
			CommandUnit& commandUnit = units++;

			if(topDrawCall.usedImages != commandUnit.usedImages_prev){
				commandUnit.updateDescriptorSets(topDrawCall.usedImages, sampler);
				commandUnit.usedImages_prev = topDrawCall.usedImages;
			}

			recordCommand(topDrawCall.frameData, commandUnit);
			topDrawCall.frameData->resetState();

			submitCommand(commandUnit, index);

			return true;
		}

		[[nodiscard]] DrawArgs acquire(VkImageView imageView, const std::size_t count = 1){
			if(!imageView) throw std::invalid_argument("ImageView is null");

			std::shared_lock dependencyLk{frameSeekingMutex};
			ImageIndex imageIndex = getMappedImageIndex(imageView);

			while(imageIndex == InvalidImageIndex){
				if(overflowMutex.try_lock()){
					std::lock_guard lk{overflowMutex, std::adopt_lock};

					toNextFrame(currentIndex, dependencyLk);

					std::unique_lock lockWrite{imageUpdateMutex};
					usedImages.fill(nullptr);
				}

				imageIndex = getMappedImageIndex(imageView);
			}

			auto [ptr, sz, lk] = acquireValidSegments(dependencyLk, count);
			return {imageIndex, static_cast<std::uint32_t>(sz), ptr, std::move(lk)};
		}

		/**
		 * @brief Guarantee obtain all required draw data space within one call
		 */
		[[nodiscard]] std::vector<DrawArgs> acquireOnce(VkImageView imageView, std::size_t count){
			std::vector<DrawArgs> drawArgs{};
			drawArgs.reserve(count / 4 + 1);

			while(count > 0){
				const auto& rst = drawArgs.emplace_back(acquire(imageView, count));
				if(rst.validCount == 0){
					throw std::invalid_argument("Failed to acquire draw arguments");
				}
				count -= rst.validCount;
			}

			return drawArgs;
		}

	private:
		void submitCommand(const CommandUnit& commandUnit, const std::size_t index) const{
			drawCall(commandUnit, index);
		}

		[[nodiscard]] ImageIndex getMappedImageIndex(VkImageView imageView){
			std::shared_lock lockRead{imageUpdateMutex};

			for(const auto& [index, usedImage] : usedImages | std::views::enumerate){
				if(usedImage == imageView){
					return index;
				}

				if(!usedImage){
					lockRead.unlock();
					std::unique_lock lockWrite{imageUpdateMutex};

					if(!usedImage){
						usedImage = imageView;
						return index;
					}

					if(usedImage == imageView) return index;
				}
			}

			return InvalidImageIndex;
		}


		std::tuple<std::byte*, std::size_t, std::shared_lock<std::shared_mutex>> acquireValidSegments(
			std::shared_lock<std::shared_mutex>& externalLock, std::size_t count){
			std::pair<std::byte*, std::size_t> rst{};

			while(true){
				auto& frame = currentFrame();

				rst = frame.acquire(count, unitOffset);

				if(rst.second == count){
					return {rst.first, count, frame.capture()};
				}

				if(overflowMutex.try_lock()){
					std::lock_guard lk{overflowMutex, std::adopt_lock};

					toNextFrame(currentIndex, externalLock);
				}
			}
		}

		FrameData& currentFrame(){
			semaphore_indexIncrement.acquire();
			auto& f = frames[currentIndex];
			semaphore_indexIncrement.release();
			return f;
		}

		/**
		* @param currentFrameIndex atomic usage
		* @param acquirer requester thread lock
		* @param checkState
		* @return next frame
		*/
		FrameData& toNextFrame(const std::size_t currentFrameIndex, std::shared_lock<std::shared_mutex>& acquirer, const bool checkState = true){
			const std::size_t nextFrameIndex = (currentFrameIndex + 1) % BufferSize;
			FrameData& currentFrame = frames[currentFrameIndex];
			FrameData& nextFrame = frames[nextFrameIndex];

			acquirer.unlock();

			semaphore_indexIncrement.acquire();
			if(currentFrameIndex == currentIndex){
				//Check if already swapped
				{
					std::unique_lock ulk{frameSeekingMutex};

					//swap index to next buffer
					currentIndex = nextFrameIndex;

					ulk.unlock();
					acquirer.lock();

					semaphore_indexIncrement.release();
				}

				//forbid write, enter waiting stage
				//don't wait the capture of each frame
				currentFrame.close();
				{
					std::lock_guard lkq{drawQueueMutex};
					{
						std::shared_lock lk{imageUpdateMutex};
						drawCalls.emplace(&currentFrame, usedImages);
					}
				}

				//Consume if overflow
				//TODO post tasks to other thread and wait here
				if(checkState){
					while(!nextFrame.isAcquirable()){
						consumeFrame();

						if(nextFrame.transferEvent.isSet()){
							nextFrame.activate(unitOffset);
							break;
						}
					}
				}

			} else{
				semaphore_indexIncrement.release();
				acquirer.lock();
			}

			return nextFrame;
		}

		void recordCommand(FrameData* frameData, CommandUnit& commandUnit) const{
			std::uint32_t count{};

			if(frameData){
				//BUG: why dead lock here after first loop??
				// auto lk{frameData->captureUnique()};
				count = frameData->getCount();
			}

			auto barriers = commandUnit.getBarriers();

			VkDependencyInfo dependencyInfo{
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.bufferMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size()),
					.pBufferMemoryBarriers = barriers.data(),
				};

			new(commandUnit.indirectBuffer.getMappedData()) VkDrawIndexedIndirectCommand{
					.indexCount = count * IndicesGroupCount,
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

				if(count){
					frameData->getVertexStagingBuffer().copyBuffer(
								  scopedCommand, commandUnit.vertexBuffer.get(), count * unitOffset);
				}
				commandUnit.indirectBuffer.cmdFlushToDevice(scopedCommand, sizeof(VkDrawIndexedIndirectCommand));
				commandUnit.descriptorBuffer_Double.cmdFlushToDevice(scopedCommand, commandUnit.descriptorBuffer_Double.size);

				frameData->transferEvent.cmdSet(scopedCommand, dependencyInfo);
			}
		}

	public:
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


			// barriers[2] = VkBufferMemoryBarrier2{
			// 		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			// 		.pNext = nullptr,
			// 		.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			// 		.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			// 		.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			// 		.dstAccessMask = VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT,
			// 		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			// 		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			// 		.buffer = db.getTargetBuffer(),
			// 		.offset = 0,
			// 		.size = VK_WHOLE_SIZE
			// 	};

			// barriers[3] = {
			// 		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			// 		.pNext = nullptr,
			// 		.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			// 		.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_HOST_WRITE_BIT,
			// 		.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
			// 		.dstAccessMask = VK_ACCESS_2_NONE,
			// 		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			// 		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			// 		.buffer = db.getStagingBuffer(),
			// 		.offset = 0,
			// 		.size = VK_WHOLE_SIZE
			// 	};

			return barriers;
		}
	};
}

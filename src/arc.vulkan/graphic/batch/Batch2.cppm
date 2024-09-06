module;

#include <vulkan/vulkan.h>

export module Graphic.Batch2;

import Core.Vulkan.Uniform;
import Core.Vulkan.Buffer.UniformBuffer;
import Core.Vulkan.Buffer.PersistentTransferBuffer;
import Core.Vulkan.Buffer.IndexBuffer;
import Core.Vulkan.Buffer.VertexBuffer;
import Core.Vulkan.Buffer.ExclusiveBuffer;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.CommandPool;
import Core.Vulkan.Sampler;
import Core.Vulkan.DescriptorSet;
import Core.Vulkan.Semaphore;
import Core.Vulkan.Context;
import Core.Vulkan.Fence;
import Core.Vulkan.BatchData;
import Core.Vulkan.Event;

import Core.Vulkan.DescriptorBuffer;
import Core.Vulkan.DescriptorSet;
import Core.Vulkan.Preinstall;

//TODO TEMP
import Assets.Graphic;

import std;
import ext.array_queue;
import ext.circular_array;

export namespace Graphic{
	struct Batch2{
		static constexpr std::size_t MaxGroupCount{2048 * 4};
		static constexpr std::size_t BufferSize{4};

		static constexpr std::size_t MaximumAllowedSamplersSize{4};

		static constexpr std::uint32_t VerticesGroupCount{4};
		static constexpr std::uint32_t IndicesGroupCount{6};

		const Core::Vulkan::Context* context{};
		Core::Vulkan::CommandPool transientCommandPool{};
		std::ptrdiff_t unitOffset{};
		using ImageSet = std::array<VkImageView, MaximumAllowedSamplersSize>;

		static constexpr ImageIndex InvalidImageIndex{static_cast<ImageIndex>(~0U)};

		struct CommandUnit{
			Core::Vulkan::ExclusiveBuffer vertexBuffer{};
			Core::Vulkan::PersistentTransferDoubleBuffer indirectBuffer{};
			Core::Vulkan::DescriptorBuffer_Double descriptorBuffer_Double{};

			Core::Vulkan::CommandBuffer flushCommandBuffer{};
			Core::Vulkan::Fence fence{};
			ImageSet usedImages_prev{};


			[[nodiscard]] CommandUnit() = default;

			[[nodiscard]] explicit CommandUnit(const Batch2& batch)
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
				flushCommandBuffer{batch.context->device, batch.transientCommandPool},
				fence{batch.context->device, Core::Vulkan::Fence::CreateFlags::signal}
			{}

			void updateDescriptorSets(const ImageSet& imageSet) const{
				const auto end = std::ranges::find(imageSet, static_cast<VkImageView>(nullptr));
				const std::span span{imageSet.begin(), end};

				const auto imageInfos =
					Core::Vulkan::Util::getDescriptorInfoRange_ShaderRead(Assets::Sampler::textureDefaultSampler, span);

				descriptorBuffer_Double.loadImage(0, imageInfos);
				descriptorBuffer_Double.flush(descriptorBuffer_Double.size);
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
			Core::Vulkan::Event drawCompleteEvent{};

			[[nodiscard]] FrameData() = default;

			void init(const Batch2& batch){
				drawCompleteEvent = Core::Vulkan::Event{batch.context->device, false};
				vertexStagingBuffer = {
						batch.context->physicalDevice, batch.context->device, (batch.unitOffset * MaxGroupCount),
						VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
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
				currentCount = 0;
				drawCompleteEvent.reset();
			}

			void activate() noexcept{
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

		Core::Vulkan::DescriptorSetLayout layout{};
		Core::Vulkan::IndexBuffer indexBuffer{};

		ImageSet usedImages{};

		std::shared_mutex imageUpdateMutex{};
		std::mutex overflowMutex{};

		std::array<FrameData, BufferSize> frames{};
		std::size_t currentIndex{};
		std::binary_semaphore semaphore_indexIncrement{1};

		/** @brief shared lock when acquire valid draw args, unique lock when swap frame*/
		std::shared_mutex frameSeekingMutex{};

		// /** @brief Used to notify all threads waiting for an available frame*/
		// std::condition_variable_any frameSeekingConditions{};

		ext::circular_array<CommandUnit, BufferSize> flushCommandBuffers{};
		ext::array_queue<DrawCall, BufferSize> drawCalls{};
		std::mutex drawQueueMutex{};

		std::function<void(VkCommandBuffer, const VkDescriptorBufferBindingInfoEXT&)> cmdBinderBegin{};

		[[nodiscard]] Batch2() = default;

		[[nodiscard]] explicit Batch2(const Core::Vulkan::Context& context, const std::size_t vertexSize) :
			context{&context}, unitOffset{static_cast<std::ptrdiff_t>(vertexSize * VerticesGroupCount)}{
			init(&context);
		}


		[[nodiscard]] Core::Vulkan::TransientCommand obtainTransientCommand() const{
			return transientCommandPool.obtainTransient(context->device.getGraphicsQueue());
		}

		void consumeAll(){
			{
				std::shared_lock lkm{frameSeekingMutex};
				if(overflowMutex.try_lock()){
					std::lock_guard lk{overflowMutex, std::adopt_lock};

					toNextFrame(currentIndex, lkm, false);
				}
			}

			while(true){
				if(!consumeFrame())break;
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

			CommandUnit& commandUnit = flushCommandBuffers++;

			if(topDrawCall.usedImages != commandUnit.usedImages_prev){
				commandUnit.updateDescriptorSets(topDrawCall.usedImages);
				commandUnit.usedImages_prev = topDrawCall.usedImages;
			}

			recordCommand(topDrawCall.frameData, commandUnit);
			topDrawCall.frameData->resetState();

			context->commandSubmit_Graphics(commandUnit.flushCommandBuffer, commandUnit.fence.get());

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
				count -= rst.validCount;
			}

			return drawArgs;
		}

	private:
		void init(const Core::Vulkan::Context* context){
			using namespace Core::Vulkan;

			transientCommandPool = CommandPool{
					context->device,
					context->physicalDevice.queues.graphicsFamily,
					VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
				};

			indexBuffer = Util::createIndexBuffer(
				*context, transientCommandPool, Core::Vulkan::Util::BatchIndices<MaxGroupCount>);

			layout = DescriptorSetLayout{
					context->device, [](DescriptorSetLayout& layout){
						layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
						                        MaximumAllowedSamplersSize);

						layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
					}
				};

			for(auto& frame : frames){
				frame.init(*this);
			}

			for(auto& commands : flushCommandBuffers){
				commands = CommandUnit{*this};
			}
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
			std::size_t nextFrameIndex = (currentFrameIndex + 1) % BufferSize;
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
				//dont wait the capture of each frame
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

						if(nextFrame.drawCompleteEvent.isSet()){
							nextFrame.activate();
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


			auto barriers =
				getBarriars(commandUnit.vertexBuffer, commandUnit.indirectBuffer.getTargetBuffer(), commandUnit.descriptorBuffer_Double);

			VkDependencyInfo dependencyInfo{
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.pNext = nullptr,
					.dependencyFlags = 0,
					.memoryBarrierCount = 0,
					.pMemoryBarriers = nullptr,
					.bufferMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size()),
					.pBufferMemoryBarriers = barriers.data(),
					.imageMemoryBarrierCount = 0,
					.pImageMemoryBarriers = nullptr
				};

			new(commandUnit.indirectBuffer.getMappedData()) VkDrawIndexedIndirectCommand{
					.indexCount = count * IndicesGroupCount,
					.instanceCount = 1,
					.firstIndex = 0,
					.vertexOffset = 0,
					.firstInstance = 0
				};
			commandUnit.indirectBuffer.flush(sizeof(VkDrawIndexedIndirectCommand));
			commandUnit.fence.waitAndReset();

			{
				Core::Vulkan::ScopedCommand scopedCommand{
						commandUnit.flushCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
					};


				// for (auto && barrier : barriers){
				// 	std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
				// 	std::swap(barrier.srcStageMask, barrier.dstStageMask);
				// }

				// vkCmdPipelineBarrier2(scopedCommand, &dependencyInfo);

				if(count){
					frameData->getVertexStagingBuffer().copyBuffer(
						scopedCommand, commandUnit.vertexBuffer.get(), count * unitOffset);
				}

				commandUnit.indirectBuffer.cmdFlushToDevice(scopedCommand, sizeof(VkDrawIndexedIndirectCommand));
				commandUnit.descriptorBuffer_Double.cmdFlushToDevice(scopedCommand, commandUnit.descriptorBuffer_Double.size);

				// for (auto && barrier : barriers){
				// 	std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
				// 	std::swap(barrier.srcStageMask, barrier.dstStageMask);
				// }

				vkCmdPipelineBarrier2(scopedCommand, &dependencyInfo);

				cmdBinderBegin(scopedCommand->get(), commandUnit.descriptorBuffer_Double.getBindInfo(VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT));

				constexpr std::size_t off[]{0};
				vkCmdBindVertexBuffers(scopedCommand, 0, 1, commandUnit.vertexBuffer.as_data(), off);
				vkCmdBindIndexBuffer(scopedCommand, indexBuffer, 0, indexBuffer.indexType);
				vkCmdDrawIndexedIndirect(scopedCommand, commandUnit.indirectBuffer.getTargetBuffer(), 0, 1,
				                         sizeof(VkDrawIndexedIndirectCommand));



				vkCmdEndRenderPass(scopedCommand);

				for (auto && barrier : barriers){
					std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
					std::swap(barrier.srcStageMask, barrier.dstStageMask);
				}

				vkCmdPipelineBarrier2(scopedCommand, &dependencyInfo);

				frameData->drawCompleteEvent.cmdSet(scopedCommand, dependencyInfo);
			}
		}

		static constexpr auto getBarriars(VkBuffer Vertex, VkBuffer Indirect, const Core::Vulkan::DescriptorBuffer_Double& db){
			std::array<VkBufferMemoryBarrier2, 3> barriers{};
			barriers[0] = {
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

			barriers[1] = {
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


			barriers[2] = {
					.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
					.pNext = nullptr,
					.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
					.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
					.dstAccessMask = VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.buffer = db.getTargetBuffer(),
					.offset = 0,
					.size = VK_WHOLE_SIZE
				};

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

	template <typename VertexType>
	struct BatchAcquirer : AutoDrawSpaceAcquirer<VertexType, BatchAcquirer<VertexType>, Batch2::VerticesGroupCount>{

	};
}

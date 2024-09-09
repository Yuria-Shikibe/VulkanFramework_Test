module;

#include <vulkan/vulkan.h>

export module Graphic.Batch;

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


import std;
import ext.array_queue;
import ext.circular_array;

//TODO allow multiple allocation in one operation
export namespace Graphic{
	// template <typename Vertex>
	struct Batch{
		static constexpr std::size_t MaxGroupCount{2048 * 4};
		static constexpr std::size_t BufferSize{3};

		static constexpr std::size_t MaximumAllowedSamplersSize{8};

	    static constexpr std::uint32_t VerticesGroupCount{4};
	    static constexpr std::uint32_t IndicesGroupCount{6};

		using ImageIndex = std::uint8_t;
		using ImageSet = std::array<VkImageView, MaximumAllowedSamplersSize>;

		static constexpr ImageIndex InvalidImageIndex{static_cast<ImageIndex>(~0U)};

		struct CommandUnit{
			Core::Vulkan::ExclusiveBuffer vertexBuffer{};
			Core::Vulkan::PersistentTransferDoubleBuffer indirectBuffer{};

			Core::Vulkan::CommandBuffer flushCommandBuffer{};
			Core::Vulkan::Fence fence{};

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
				flushCommandBuffer{batch.context->device, batch.transientCommandPool},
				fence{batch.context->device, Core::Vulkan::Fence::CreateFlags::signal}{}
		};

		struct FrameData{
			static constexpr std::uint32_t MaxCount = MaxGroupCount;

			Core::Vulkan::Event drawCompleteEvent{};
			Core::Vulkan::StagingBuffer stagingBuffer{};
			void* vertData{};

			std::atomic_uint32_t currentCount{};
			std::atomic_bool canAcquire{true};

			std::shared_mutex capturedMutex{};

			[[nodiscard]] FrameData() = default;

			void set(const Batch& batch){
				drawCompleteEvent = Core::Vulkan::Event{batch.context->device, false};
				stagingBuffer = {
					batch.context->physicalDevice, batch.context->device, (batch.unitOffset * MaxGroupCount),
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				};
				vertData = stagingBuffer.memory.map_noInvalidation();
			}

			[[nodiscard]] bool isAcquirable() const{
				return canAcquire;
			}

			void reset(){
				currentCount.store(0);
				canAcquire = true;
				drawCompleteEvent.reset();
			}

			void close(){
				canAcquire = false;
			}

			std::pair<void*, bool> acquire(const std::ptrdiff_t unitOffset){
				const auto offset = (currentCount++);
				const bool v = offset < MaxCount;

				return {static_cast<std::uint8_t*>(vertData) + offset * unitOffset, v};
			}
		};

		struct DrawCall{
			FrameData* frameData{};
			ImageSet usedImages{};

			void updateDescriptorImages(Batch& batch) const {
				if(usedImages != batch.usedImages_old){

					batch.updateDescriptorSets(usedImages);

					batch.usedImages_old = usedImages;
				}
			}
		};

		struct DrawArgs{
			ImageIndex imageIndex{};
			void* dataPtr{};
			std::shared_lock<std::shared_mutex> captureLock{};
		};

	    const Core::Vulkan::Context* context{};
	    std::ptrdiff_t unitOffset{};


	    Core::Vulkan::CommandPool transientCommandPool{};


	    //Buffers to be bound
	    Core::Vulkan::IndexBuffer indexBuffer{};
	    Core::Vulkan::PersistentTransferDoubleBuffer indirectBuffer{};
	    Core::Vulkan::ExclusiveBuffer vertexBuffer{};

		Core::Vulkan::Event transferCompleteEvent{};
		Core::Vulkan::Event drawCompleteEvent{};

		Core::Vulkan::Semaphore vertSemaphore{};
		Core::Vulkan::Semaphore drawSemaphore{};

	    ImageSet usedImages_old{};
	    ImageSet usedImages{};
	    std::shared_mutex imageMutex{};

		bool descriptorSetUpdated{false};
		// Core::Vulkan::Fence drawCommandFence{};

		std::array<FrameData, BufferSize> frames{};
		std::atomic_size_t currentIndex{};
		std::binary_semaphore semaphore_indexIncrement{1};

		/** @brief shared lock when acquire valid draw args, unique lock when swap frame*/
		std::shared_mutex frameSeekingMutex{};
		/** @brief Used to notify all threads waiting for an available frame*/
		std::condition_variable_any frameSeekingConditions{};


		ext::circular_array<CommandUnit, 1> flushCommandBuffers{};
		ext::array_queue<DrawCall, BufferSize> drawCalls{};
		std::binary_semaphore semaphore_drawQueuePush{1};

		//TODO better callback
		std::function<void(Batch&)> externalDrawCall{};
		std::function<void(Batch&, std::span<const VkImageView>)> descriptorChangedCallback{};
		std::function<void(VkCommandBuffer)> cmdBinderBegin{};
		std::function<void(VkCommandBuffer)> cmdBinderEnd{};

        [[nodiscard]] Batch() = default;

        [[nodiscard]] explicit Batch(const Core::Vulkan::Context& context, const std::size_t vertexSize) :
            context{&context} {
            setUnitOffset(vertexSize);
            init(&context);
        }

        void setUnitOffset(const std::size_t vertexSize) {
	        unitOffset = vertexSize * VerticesGroupCount;
	    }

	    [[nodiscard]] Core::Vulkan::TransientCommand obtainTransientCommand() const {
            return transientCommandPool.obtainTransient(context->device.getPrimaryGraphicsQueue());
        }

		[[nodiscard]] DrawArgs acquire(VkImageView imageView){
			if(!imageView)throw std::invalid_argument("ImageView is null");
        	std::shared_lock dependencyLk{frameSeekingMutex};
			ImageIndex imageIndex = getMappedImageIndex(imageView);

			if(imageIndex == InvalidImageIndex){

				toNextFrame(currentIndex, dependencyLk);

				{
					std::unique_lock lockWrite{imageMutex};
					usedImages.fill(nullptr);
					usedImages[0] = imageView;
				}
				imageIndex = 0;
			}

			auto [ptr, lk] = obtainData(dependencyLk);
			return {imageIndex, ptr, std::move(lk)};
		}

		FrameData& currentFrame(){
			return frames[currentIndex];
		}

		[[nodiscard]] BatchData getBatchData() const noexcept{
			return {
				.vertices = vertexBuffer,
				.indices = indexBuffer,
				.indexType = indexBuffer.indexType,
				.indirect = indirectBuffer.getTargetBuffer()
			};
		}

		void updateDescriptorSets(const ImageSet& imageSet = {}) {
            auto end = std::ranges::find(imageSet, static_cast<VkImageView>(nullptr));

            std::span span{imageSet.begin(), end};

        	descriptorSetUpdated = true;
			descriptorChangedCallback(*this, span);
		}

		/**
		 * @brief multi call is allowed, do nothing when there is no frame valid
		 */
		void consumeFrame(){
			beginPost(); //ensure queue is thread safe
			if(drawCalls.empty()){
				endPost();
				return;
			}

        	const auto index = flushCommandBuffers.get_index();
			const DrawCall topDrawCall = drawCalls.front();
			drawCalls.pop();
			endPost();

			auto& commandUnit = flushCommandBuffers++;

			topDrawCall.updateDescriptorImages(*this);
			this->recordCommand(topDrawCall.frameData, commandUnit);

			submitCommand(commandUnit);

			// topDrawCall.frameData->reset();
			frameSeekingConditions.notify_all();
		}

		void consumeAll(){
			{
				std::shared_lock lk{frameSeekingMutex};
			    (void)toNextFrame(currentIndex, lk);
			}

			while(!drawCalls.empty()){
				consumeFrame();
			}
		}

	private:
		void submitCommand(const CommandUnit& commandUnit){
			context->commandSubmit_Graphics(commandUnit.flushCommandBuffer, commandUnit.fence.get());

			// commandUnit.fence.wait();
			//
			// if(descriptorSetUpdated) [[unlikely]] {
			// 	descriptorSetUpdated = false;
			// }else{
			// 	//OPTM no wait
			// 	// auto time = std::chrono::steady_clock::now();
			// 	drawCommandFence.waitAndReset();
			// 	// auto time1 = std::chrono::steady_clock::now();
			// 	// std::println("{}", std::chrono::duration_cast<std::chrono::nanoseconds>(time1 - time));
			// }

			externalDrawCall(*this);
			// commandUnit.fence.wait();
		}

        /**
		 * @param currentFrameIndex atomic usage
		 * @param acquirer requester thread lock
		 * @return next frame
		 */
		FrameData& toNextFrame(const std::size_t currentFrameIndex, std::shared_lock<std::shared_mutex>& acquirer){
			FrameData& frame = currentFrame();
			FrameData& nextFrame = frames[(currentFrameIndex + 1) % BufferSize];

			acquirer.unlock();

			semaphore_indexIncrement.acquire();
			if(currentFrameIndex == currentIndex){ //Check if already swapped
				{
					std::unique_lock ulk{frameSeekingMutex};

					//swap index to next buffer
					currentIndex = (currentFrameIndex + 1) % BufferSize;

					ulk.unlock();
					acquirer.lock();

					semaphore_indexIncrement.release();
				}

				//forbid write, enter waiting stage
				frame.close();

				//Only post when successfully swapped
				beginPost(); //ensure queue is thread safe
				{
					std::shared_lock lk{imageMutex};
					drawCalls.push(DrawCall{&frame, usedImages});
				}
				endPost();

				//Consume if overflow
				//TODO post tasks to other thread and wait here

				while(!nextFrame.isAcquirable()){

					std::println(std::cerr, "blocked");
					consumeFrame();
					if(nextFrame.drawCompleteEvent.isSet()){
						nextFrame.reset();
						break;
					}
				}

			} else{
				semaphore_indexIncrement.release();
				acquirer.lock();
			}

			return nextFrame;
		}

		void beginPost(){
			semaphore_drawQueuePush.acquire();
		}

		void endPost(){
			semaphore_drawQueuePush.release();
		}

	    [[nodiscard]] ImageIndex getMappedImageIndex(VkImageView imageView){
		    std::shared_lock lockRead{imageMutex};

		    for(const auto& [index, usedImage] : usedImages | std::views::enumerate){
		        if(usedImage == imageView){
		            return index;
		        }

		        if(!usedImage){
		            lockRead.unlock();
		            std::unique_lock lockWrite{imageMutex};

		            if(!usedImage){
		                usedImage = imageView;
		                return index;
		            }

		            if(usedImage == imageView)return index;
		        }
		    }

		    return InvalidImageIndex;
		}

		std::pair<void*, std::shared_lock<std::shared_mutex>> obtainData(std::shared_lock<std::shared_mutex>& externalLock){
			std::pair<void*, bool> rst{};

			while(true){
				auto& frame = currentFrame();

				rst = frame.acquire(unitOffset);

				if(rst.second) return {rst.first, std::shared_lock{frame.capturedMutex}};

				//release waiter: swap required
				toNextFrame(currentIndex, externalLock);

				// throw std::overflow_error{"Batch Overflow"};
				//Wait until draw complete and the frame is released
				//TODO stop token
				// frameSeekingConditions.wait(externalLock, [&]{
				// 	return !frame.isAcquirable();
				// });
			}
		}

		void recordCommand(FrameData* frameData, CommandUnit& commandUnit){
			std::uint32_t count{};

			if(frameData){
				std::unique_lock lk{frameData->capturedMutex};
				count = std::min(FrameData::MaxCount, frameData->currentCount.load());
			}


			std::array<VkBufferMemoryBarrier2, 2> barriers = getBarriars(commandUnit.vertexBuffer, commandUnit.indirectBuffer.getTargetBuffer());

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
			commandUnit.indirectBuffer.flush(VK_WHOLE_SIZE);
			commandUnit.fence.waitAndReset();

			{
				Core::Vulkan::ScopedCommand scopedCommand{commandUnit.flushCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};


				// vkCmdPipelineBarrier2(scopedCommand, &dependencyInfo);

				// drawCompleteEvent.cmdWait(scopedCommand, dependencyInfo);
				// drawCompleteEvent.cmdReset(scopedCommand, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT);

				std::swap(barriers[0].srcAccessMask, barriers[0].dstAccessMask);
				std::swap(barriers[1].srcAccessMask, barriers[1].dstAccessMask);
				std::swap(barriers[0].srcStageMask, barriers[0].dstStageMask);
				std::swap(barriers[1].srcStageMask, barriers[1].dstStageMask);

				if(count){
					frameData->stagingBuffer.copyBuffer(scopedCommand, commandUnit.vertexBuffer.get(), count * unitOffset);
				}

				commandUnit.indirectBuffer.cmdFlushToDevice(scopedCommand, sizeof(VkDrawIndexedIndirectCommand));

				vkCmdPipelineBarrier2(scopedCommand, &dependencyInfo);

				std::swap(barriers[0].srcAccessMask, barriers[0].dstAccessMask);
				std::swap(barriers[1].srcAccessMask, barriers[1].dstAccessMask);
				std::swap(barriers[0].srcStageMask, barriers[0].dstStageMask);
				std::swap(barriers[1].srcStageMask, barriers[1].dstStageMask);
				// std::swap(barrier2.srcAccessMask, barrier2.dstAccessMask);
				// std::swap(barrier2.srcStageMask, barrier2.dstStageMask);

				cmdBinderBegin(scopedCommand->get());

				constexpr std::size_t off[]{0};
				vkCmdBindVertexBuffers(scopedCommand, 0, 1, commandUnit.vertexBuffer.as_data(), off);
				vkCmdBindIndexBuffer(scopedCommand, indexBuffer, 0, indexBuffer.indexType);
				vkCmdDrawIndexedIndirect(scopedCommand, commandUnit.indirectBuffer.getTargetBuffer(), 0, 1, sizeof(VkDrawIndexedIndirectCommand));

				cmdBinderEnd(scopedCommand->get());

				// vkCmdPipelineBarrier2(scopedCommand, &dependencyInfo);

				frameData->drawCompleteEvent.cmdSet(scopedCommand, dependencyInfo);
			}
		}

	    void init(const Core::Vulkan::Context* context){
		    using namespace Core::Vulkan;

			// transferCompleteEvent = Event{context->device, true};
			drawCompleteEvent = Event{context->device, true};

			vertSemaphore = Semaphore{context->device};
			drawSemaphore = Semaphore{context->device};
			// drawSemaphore.signal();

		    transientCommandPool = CommandPool{
		        context->device,
                context->graphicFamily(),
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
            };

			// drawCommandFence = Fence{context->device, Fence::CreateFlags::noSignal};
		    // vertTransferCommandFence = Fence{context->device, Fence::CreateFlags::signal};

		    indexBuffer = Util::createIndexBuffer(
                *context, transientCommandPool, Core::Vulkan::Util::BatchIndices<MaxGroupCount>);

		    indirectBuffer = PersistentTransferDoubleBuffer{
		        context->physicalDevice, context->device,
                sizeof(VkDrawIndexedIndirectCommand),
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT};

		    // command_flush = transientCommandPool.obtain();

		    vertexBuffer = ExclusiveBuffer{
		        context->physicalDevice, context->device,
                (unitOffset * MaxGroupCount),
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};

		    for(auto& frame : frames){
		    	frame.set(*this);
		    }

		    for(auto& commands : flushCommandBuffers){
		    	commands = CommandUnit{*this};
		    }


			/*auto barriers = getBarriars();
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

			{


		    	auto command_empty = transientCommandPool.obtain();
			    ScopedCommand scopedCommand{command_empty, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

			    drawCompleteEvent.cmdSet(command_empty, dependencyInfo);
			    // context->triggerSemaphore(command_empty, {drawSemaphore.get()});
			}*/

		}

		auto getBarriars(VkBuffer Vertex, VkBuffer Indirect) const{
			std::array<VkBufferMemoryBarrier2, 2> barriers{};
			barriers[0] = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
				.pNext = nullptr,

				.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
				.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,

				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.buffer = Indirect,
				.offset = 0,
				.size = VK_WHOLE_SIZE
			};

			barriers[1] = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
				.pNext = nullptr,
				.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
				.srcAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.buffer = Vertex,
				.offset = 0,
				.size = VK_WHOLE_SIZE
			};

			return barriers;
		}
	};
}

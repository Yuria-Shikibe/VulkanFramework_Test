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

import std;
import ext.Container.ArrayQueue;
import ext.circular_array;

//TODO allow multiple allocation in one operation
export namespace Graphic{
	// template <typename Vertex>
	struct Batch{
		static constexpr std::size_t MaxGroupCount{8192};
		static constexpr std::size_t BufferSize{3};

		static constexpr std::size_t MaximumAllowedSamplersSize{8};

	    static constexpr std::uint32_t VerticesGroupCount{4};
	    static constexpr std::uint32_t IndicesGroupCount{6};

		using ImageIndex = std::uint8_t;
		using ImageSet = std::array<VkImageView, MaximumAllowedSamplersSize>;

		static constexpr ImageIndex InvalidImageIndex{static_cast<ImageIndex>(~0U)};

		struct CommandUnit{
			Core::Vulkan::CommandBuffer flushCommandBuffer{};
			Core::Vulkan::Fence fence{};

			[[nodiscard]] CommandUnit() = default;

			[[nodiscard]] explicit CommandUnit(const Batch& batch)
				:
				flushCommandBuffer{batch.context->device, batch.transientCommandPool},
				fence{batch.context->device, Core::Vulkan::Fence::CreateFlags::signal}{}
		};

		struct FrameData{
			static constexpr std::uint32_t MaxCount = MaxGroupCount;

			Core::Vulkan::StagingBuffer stagingBuffer{};
			void* vertData{};

			std::atomic_uint32_t currentCount{};
			std::atomic_bool canAcquire{true};

			std::shared_mutex capturedMutex{};

			[[nodiscard]] FrameData() = default;

			void set(const Batch& batch){
				stagingBuffer = {
					batch.context->physicalDevice, batch.context->device, (batch.unitOffset * MaxGroupCount),
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
				};
				vertData = stagingBuffer.memory.map_noInvalidation();
			}

			[[nodiscard]] bool isAcquirable() const{
				return canAcquire;
			}

			void reset(){
				currentCount.store(0);
				canAcquire = true;
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


	    ImageSet usedImages_old{};
	    ImageSet usedImages{};
	    std::shared_mutex imageMutex{};

		bool descriptorSetUpdated{false};
		Core::Vulkan::Fence drawCommandFence{};

		std::array<FrameData, BufferSize> frames{};
		std::atomic_size_t currentIndex{};
		std::binary_semaphore semaphore_indexIncrement{1};

		/** @brief shared lock when acquire valid draw args, unique lock when swap frame*/
		std::shared_mutex frameSeekingMutex{};
		/** @brief Used to notify all threads waiting for an available frame*/
		std::condition_variable_any frameSeekingConditions{};


		ext::circular_array<CommandUnit, BufferSize> flushCommandBuffers{};
		ext::array_queue<DrawCall, BufferSize> drawCalls{};
		std::binary_semaphore semaphore_drawQueuePush{1};

		//TODO better callback
		std::function<void(Batch&)> externalDrawCall{};
		std::function<void(Batch&, std::span<const VkImageView>)> descriptorChangedCallback{};

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
            return transientCommandPool.obtainTransient(context->device.getGraphicsQueue());
        }

		[[nodiscard]] DrawArgs acquire(VkImageView imageView){
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

		[[nodiscard]] Core::Vulkan::BatchData getBatchData() const noexcept{
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

			const DrawCall topDrawCall = drawCalls.front();
			drawCalls.pop();
			endPost();

			auto& commandUnit = flushCommandBuffers++;

			this->recordCommand(topDrawCall.frameData, commandUnit);
			topDrawCall.updateDescriptorImages(*this);

			submitCommand(commandUnit);

			topDrawCall.frameData->reset();
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

			if(descriptorSetUpdated) [[unlikely]] {
				descriptorSetUpdated = false;
			}else{
				//OPTM no wait
				// auto time = std::chrono::steady_clock::now();
				drawCommandFence.waitAndReset();
				// auto time1 = std::chrono::steady_clock::now();
				// std::println("{}", std::chrono::duration_cast<std::chrono::nanoseconds>(time1 - time));
			}

			externalDrawCall(*this);
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
				if(!nextFrame.isAcquirable())consumeFrame();
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
				if(frame.isAcquirable()){
					rst = frame.acquire(unitOffset);

					if(rst.second) return {rst.first, std::shared_lock{frame.capturedMutex}};

					//release waiter: swap required
					toNextFrame(currentIndex, externalLock);
				} else{
					//Wait until draw complete and the frame is released
					//TODO stop token
					frameSeekingConditions.wait(externalLock, [&]{
						return !frame.isAcquirable();
					});
				}
			}
		}

		void recordCommand(FrameData* frameData, CommandUnit& commandUnit){
			std::uint32_t count{};

			if(frameData){
				std::unique_lock lk{frameData->capturedMutex};
				count = std::min(FrameData::MaxCount, frameData->currentCount.load());
			}

			commandUnit.fence.waitAndReset();

			Core::Vulkan::ScopedCommand scopedCommand{commandUnit.flushCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

			vkCmdPipelineBarrier(scopedCommand,
				VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				0, nullptr);

			if(count){
				frameData->stagingBuffer.copyBuffer(scopedCommand, vertexBuffer.get(), count * unitOffset);
			}

			new(indirectBuffer.getMappedData()) VkDrawIndexedIndirectCommand{
				.indexCount = count * IndicesGroupCount,
				.instanceCount = 1,
				.firstIndex = 0,
				.vertexOffset = 0,
				.firstInstance = 0
			};

			indirectBuffer.flush(sizeof(VkDrawIndexedIndirectCommand));
			indirectBuffer.cmdFlushToDevice(scopedCommand, sizeof(VkDrawIndexedIndirectCommand));

			vkCmdPipelineBarrier(scopedCommand,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
				0,
				0,
				nullptr,
				0,
				nullptr,
				0,
				nullptr);
		}

	    void init(const Core::Vulkan::Context* context){
		    using namespace Core::Vulkan;

		    transientCommandPool = CommandPool{
		        context->device,
                context->physicalDevice.queues.graphicsFamily,
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
            };

			/*{
				semaphore_copyDone = Semaphore{context->device};
		    	semaphore_drawDone = Semaphore{context->device};

		    	auto command_empty = transientCommandPool.obtain();
			    {ScopedCommand scopedCommand{command_empty, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};}
		    	context->triggerSemaphore(command_empty, semaphore_drawDone.asSeq());
			}*/

			drawCommandFence = Fence{context->device, Fence::CreateFlags::signal};
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

		}
	};
}

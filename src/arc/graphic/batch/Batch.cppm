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

import std;

export namespace Graphic{
	template <typename Vertex, std::size_t maxGroupCount = 8192, std::size_t bufferSize = 3>
	struct Batch{
		static constexpr std::size_t MaximumAllowedSamplersSize{8};

		using ImageIndex = std::uint32_t;
		using ImageSet = std::array<VkImageView, MaximumAllowedSamplersSize>;

		static constexpr std::uint32_t VerticesGroupCount{4};
		static constexpr std::uint32_t IndicesGroupCount{6};

		static constexpr ImageIndex InvalidImageIndex{~0U};
		static constexpr std::ptrdiff_t UnitOffset = sizeof(Vertex) * VerticesGroupCount;

		Core::Vulkan::Context* context{};

		Core::Vulkan::CommandPool transientCommandPool{};
		Core::Vulkan::CommandPool commandPool{};

		VkSampler sampler{};

		//Buffers to be bound
		Core::Vulkan::IndexBuffer buffer_index{};
		Core::Vulkan::PersistentTransferDoubleBuffer buffer_indirect{};
		Core::Vulkan::ExclusiveBuffer buffer_vertex{};

		Core::Vulkan::CommandBuffer command_flush{};
		Core::Vulkan::CommandBuffer command_empty{};
		Core::Vulkan::CommandBuffer command_rebindDescriptorSet{};

		ImageSet usedImages_old{};
		ImageSet usedImages{};
		std::shared_mutex imageMutex{};

		Core::Vulkan::Semaphore semaphore_drawDone{};
		Core::Vulkan::Semaphore semaphore_copyDone{};
		Core::Vulkan::Fence fence{};

		struct FrameData{
			static constexpr std::uint32_t MaxCount = maxGroupCount;

			Core::Vulkan::StagingBuffer stagingBuffer{};
			void* vertData{};

			std::atomic_uint32_t currentCount{};
			std::atomic_bool canAcquire{true};

			std::shared_mutex capturedMutex{};

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

			std::pair<void*, bool> acquire(){
				const auto offset = currentCount++;
				const bool v = offset < MaxCount;

				return {static_cast<std::uint8_t*>(vertData) + offset * UnitOffset, v};
			}
		};

		struct DrawCall{
			FrameData* frameData{};
			ImageSet usedImages{};

			void updateDescriptorImages(Batch& batch){
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

		std::array<FrameData, bufferSize> frames{};
		std::atomic_size_t currentIndex{};
		std::binary_semaphore semaphore_indexIncrement{1};

		/** @brief shared lock when acquire valid draw args, unique lock when swap frame*/
		std::shared_mutex frameSeekingMutex{};
		/** @brief Used to notify all threads waiting for an available frame*/
		std::condition_variable_any frameSeekingConditions{};

		//TODO make it static size, avoid heap allocation
		std::queue<DrawCall> drawCalls{};
		std::binary_semaphore semaphore_drawQueuePush{1};

		//TODO better callback
		std::function<void(Batch&, bool)> externalDrawCall{};
		std::function<void(std::span<VkDescriptorImageInfo>)> descriptorChangedCallback{};

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

		[[nodiscard]] DrawArgs getDrawArgs(VkImageView imageView){
			std::shared_lock dependencyLk{frameSeekingMutex};
			ImageIndex imageIndex = getMappedImageIndex(imageView);

			if(imageIndex == InvalidImageIndex){

				swapFrame(currentIndex, dependencyLk);

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

		void init(Core::Vulkan::Context* context, VkSampler sampler){
			using namespace Core::Vulkan;
			this->context = context;
			this->sampler = sampler;

			transientCommandPool = CommandPool{
					context->device,
					context->physicalDevice.queues.graphicsFamily,
					VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
				};

			commandPool = CommandPool{
					context->device,
					context->physicalDevice.queues.graphicsFamily,
					VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
				};

			semaphore_copyDone = Semaphore{context->device};
			semaphore_drawDone = Semaphore{context->device};
			fence = Fence{context->device, Fence::CreateFlags::signal};

			command_empty = commandPool.obtain();
			{ScopedCommand scopedCommand{command_empty, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};}
			context->triggerSemaphore(command_empty, semaphore_drawDone.asSeq());
			command_rebindDescriptorSet = commandPool.obtain(VK_COMMAND_BUFFER_LEVEL_SECONDARY);


			buffer_index = Util::createIndexBuffer(
				*context, transientCommandPool, Core::Vulkan::Util::BatchIndices<maxGroupCount>);

			buffer_indirect = PersistentTransferDoubleBuffer{
				context->physicalDevice, context->device,
				sizeof(VkDrawIndexedIndirectCommand),
				VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT};

			command_flush = transientCommandPool.obtain();

			buffer_vertex = ExclusiveBuffer{
				context->physicalDevice, context->device,
				(UnitOffset * maxGroupCount),
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};

			for(auto& frame : frames){
				frame.stagingBuffer = StagingBuffer{
						context->physicalDevice, context->device, (UnitOffset * maxGroupCount),
						VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
					};

				frame.vertData = frame.stagingBuffer.memory.map_noInvalidation();
			}
		}

		void updateDescriptorSets(const ImageSet& imageSet = {}){
			auto imageInfo = Core::Vulkan::Util::getDescriptorInfoRange_ShaderRead(sampler,
				imageSet | std::views::take_while([](VkImageView imageView){ return imageView != nullptr; }));

			descriptorChangedCallback(imageInfo);
		}

		class [[jetbrains::guard]] Acquirer{
			DrawArgs drawArgs{};

			[[nodiscard("Captured Shared Mutex Should be reserved until draw is done")]]
			Acquirer(Batch& batch, VkImageView imageView) :
				drawArgs{batch.getDrawArgs(imageView)}{}

			[[nodiscard]] std::pair<void*, ImageIndex> getArgs() const noexcept{
				return {drawArgs.dataPtr, drawArgs.imageIndex};
			}

			[[nodiscard]] operator std::pair<void*, ImageIndex>() const noexcept{
				return getArgs();
			}
		};

		/**
		 * @brief multi call is allowed, do nothing when there is no frame valid
		 */
		void consumeFrame(){
			beginPost(); //ensure queue is thread safe
			if(drawCalls.empty()){
				endPost();
				return;
			}

			DrawCall topDrawCall = drawCalls.front();
			drawCalls.pop();
			bool isLast = drawCalls.empty();
			endPost();

			this->recordCommand(topDrawCall.frameData);
			topDrawCall.updateDescriptorImages(*this);

			submitCommand(isLast);

			topDrawCall.frameData->reset();
			frameSeekingConditions.notify_all();
		}

		void consumeAll(){
			{
				std::shared_lock lk{frameSeekingMutex};
				auto& frame = swapFrame(currentIndex, lk);
			}

			if(drawCalls.empty()){
				//padding when no element to draw

				recordCommand(nullptr);

				submitCommand(true);

			}else{
				while(!drawCalls.empty()){
					consumeFrame();
				}
			}
		}

	private:
		void submitCommand(const bool isLast){
			// fence.reset();

			context->commandSubmit_Graphics(
					command_flush,
					semaphore_drawDone,
					semaphore_copyDone,
					fence.get(),
					VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
					VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
				);

			externalDrawCall(*this, isLast);
		}

		FrameData& swapFrame(const std::size_t currentFrameIndex, std::shared_lock<std::shared_mutex>& acquirer){
			FrameData& frame = currentFrame();
			FrameData& nextFrame = frames[(currentFrameIndex + 1) % bufferSize];

			acquirer.unlock();

			semaphore_indexIncrement.acquire();
			if(currentFrameIndex == currentIndex){ //Check if already swapped
				{
					std::unique_lock ulk{frameSeekingMutex};

					//swap index to next buffer
					currentIndex = (currentFrameIndex + 1) % bufferSize;

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

		std::pair<void*, std::shared_lock<std::shared_mutex>> obtainData(std::shared_lock<std::shared_mutex>& externalLock){
			std::pair<void*, bool> rst{};

			while(true){
				auto& frame = currentFrame();
				if(frame.isAcquirable()){
					rst = frame.acquire();

					if(rst.second) return {rst.first, std::shared_lock{frame.capturedMutex}};

					//release waiter: swap required
					swapFrame(currentIndex, externalLock);
				} else{
					//Wait until draw complete and the frame is released
					//TODO stop token
					frameSeekingConditions.wait(externalLock, [&]{
						return !frame.isAcquirable();
					});
				}
			}
		}

		void recordCommand(FrameData* frameData){
			std::uint32_t count{};

			fence.waitAndReset();
			command_flush.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			if(frameData){
				std::unique_lock lk{frameData->capturedMutex};

				count = std::min(FrameData::MaxCount, frameData->currentCount.load());
				if(count){
					static_cast<Core::Vulkan::StagingBuffer&>(frameData->stagingBuffer)
						.copyBuffer(command_flush, buffer_vertex.get(), count * UnitOffset);


					VkBufferMemoryBarrier bufferMemoryBarrier{
						.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
						.pNext = nullptr,
						.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
						.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
						.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.buffer = buffer_vertex,
						.offset = 0,
						.size = static_cast<VkDeviceSize>(count * UnitOffset)
					};

					vkCmdPipelineBarrier(
						command_flush,
						VK_PIPELINE_STAGE_TRANSFER_BIT,                                           // srcStageMask
						VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, // dstStageMask
						0,
						0, nullptr,
						1,
						&bufferMemoryBarrier, // pBufferMemoryBarriers
						0, nullptr
					);

				}
			}

			new(buffer_indirect.getMappedData()) VkDrawIndexedIndirectCommand{
				.indexCount = count * IndicesGroupCount,
				.instanceCount = 1,
				.firstIndex = 0,
				.vertexOffset = 0,
				.firstInstance = 0
			};

			buffer_indirect.flush(sizeof(VkDrawIndexedIndirectCommand));
			buffer_indirect.cmdFlushToDevice(command_flush, sizeof(VkDrawIndexedIndirectCommand));

			VkBufferMemoryBarrier indirectBufferMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.buffer = buffer_indirect.getTargetBuffer(),
				.offset = 0,
				.size = sizeof(VkDrawIndexedIndirectCommand)
			};

			vkCmdPipelineBarrier(
				command_flush,
				VK_PIPELINE_STAGE_TRANSFER_BIT,                                           // srcStageMask
				VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, // dstStageMask
				0,
				0, nullptr,
				1,
				&indirectBufferMemoryBarrier, // pBufferMemoryBarriers
				0, nullptr
			);

			command_flush.end();
		}
	};
}

module;

#include <vulkan/vulkan.h>

export module Graphic.Batch.MultiThread;

export import Graphic.Batch.Base;

import std;
import ext.array_queue;
import ext.circular_array;

import ext.cond_atomic;

export namespace Graphic{
	class LockableVerticesData : public UniversalVerticesData<true>{
		std::shared_mutex capturedMutex{};

	public:
		[[nodiscard]] LockableVerticesData() = default;

		[[nodiscard]] std::shared_lock<decltype(capturedMutex)> capture(){
			return std::shared_lock{capturedMutex};
		}

		[[nodiscard]] std::pair<bool, std::unique_lock<decltype(capturedMutex)>> tryCaptureUnique(){
			if(capturedMutex.try_lock()){
				return {true, std::unique_lock{capturedMutex, std::adopt_lock}};
			}else{
				return {false, {}};
			}
		}
		[[nodiscard]] std::unique_lock<decltype(capturedMutex)> captureUnique(){
			return std::unique_lock{capturedMutex};
		}
	};


	//TODO support move assignment
	//OPTM use shared index buffer instead of exclusive one
	struct Batch_MultiThread : BasicBatch<LockableVerticesData>{
	private:
		std::shared_mutex imageUpdateMutex{};

		std::mutex overflowMutex{};

		std::binary_semaphore semaphore_indexIncrement{1};

		/** @brief shared lock when acquire valid draw args, unique lock when swap frame*/
		std::shared_mutex frameSeekingMutex{};

		std::mutex drawQueueMutex{};

	public:
		using BasicBatch<LockableVerticesData>::BasicBatch;

		void consumeAll(){
			while(true){
				if(!consumeOne())break;
			}

			{
				std::shared_lock lkm{frameSeekingMutex};
				if(overflowMutex.try_lock()){
					std::lock_guard lk{overflowMutex, std::adopt_lock};
					if(!currentVerticesData().empty()){
						toNextVerticesData(currentIndex, lkm, true);
						consumeOne();
					}
				}
			}
		}

		/**
		 * @brief multi call is allowed, do nothing when there is no frame valid
		 */
		bool consumeOne(){
			SubmittedDrawCall topDrawCall;

			{
				std::lock_guard lkq{drawQueueMutex};

				if(pendingDrawCalls.empty()){
					return false;
				}

				topDrawCall = pendingDrawCalls.front();
				pendingDrawCalls.pop();
			}

			auto index = units.get_index();
			CommandUnit& commandUnit = units++;

			if(topDrawCall.usedImages != commandUnit.usedImages_prev){
				commandUnit.updateDescriptorSets(topDrawCall.usedImages, sampler);
				commandUnit.usedImages_prev = topDrawCall.usedImages;
			}

			recordTransferCommand(topDrawCall.frameData, commandUnit);
			topDrawCall.frameData->setTransferring();

			submitCommand(commandUnit, index);

			return true;
		}

		[[nodiscard]] LockableDrawArgs acquire(VkImageView imageView, const std::size_t count = 1){
			if(!imageView) throw std::invalid_argument("ImageView is null");

			std::shared_lock dependencyLk{frameSeekingMutex};
			ImageIndex imageIndex = getMappedImageIndex(imageView);

			while(imageIndex == InvalidImageIndex){
				if(overflowMutex.try_lock()){
					std::lock_guard lk{overflowMutex, std::adopt_lock};

					toNextVerticesData(currentIndex, dependencyLk);

					std::unique_lock lockWrite{imageUpdateMutex};
					clearImageState();
				}

				imageIndex = getMappedImageIndex(imageView);
			}

			auto [ptr, sz, lk] = acquireValidSegments(dependencyLk, count);
			return {imageIndex, static_cast<std::uint32_t>(sz), ptr, std::move(lk)};
		}

		/**
		 * @brief Guarantee obtain all required draw data space within one call
		 */
		[[nodiscard]] std::vector<LockableDrawArgs> acquireOnce(VkImageView imageView, std::size_t count){
			std::vector<LockableDrawArgs> drawArgs{};
			drawArgs.reserve(count / 4 + 1);

			while(count > 0){
				const auto& rst = drawArgs.emplace_back(acquire(imageView, count));
				if(rst.validCount == 0){
					throw std::invalid_argument("unable to acquire draw arguments");
				}
				count -= rst.validCount;
			}

			return drawArgs;
		}

	private:
		[[nodiscard]] ImageIndex getMappedImageIndex(VkImageView imageView){
			std::shared_lock lockRead{imageUpdateMutex};

			if(imageView == lastUsedImageView){
				return lastImageIndex;
			}

			for(const auto& [index, usedImage] : currentUsedImages | std::views::enumerate){
				if(usedImage == imageView){
					return index;
				}

				if(!usedImage){
					lockRead.unlock();
					std::unique_lock lockWrite{imageUpdateMutex};

					if(!usedImage){
						usedImage = lastUsedImageView = imageView;
						lastImageIndex = index;
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
				auto& frame = currentVerticesData();

				rst = frame.acquire(count, unitOffset);

				if(rst.second == count){
					return {rst.first, count, frame.capture()};
				}

				if(overflowMutex.try_lock()){
					std::lock_guard lk{overflowMutex, std::adopt_lock};

					toNextVerticesData(currentIndex, externalLock);
				}
			}
		}

		LockableVerticesData& currentVerticesData() noexcept{
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
		LockableVerticesData& toNextVerticesData(const std::size_t currentFrameIndex, std::shared_lock<std::shared_mutex>& acquirer, const bool checkState = true){
			const std::size_t nextFrameIndex = (currentFrameIndex + 1) % BufferSize;
			LockableVerticesData& currentFrame = frames[currentFrameIndex];
			LockableVerticesData& nextFrame = frames[nextFrameIndex];

			acquirer.unlock();

			semaphore_indexIncrement.acquire();
			if(currentFrameIndex == currentIndex){

				//Consume if overflow
				//TODO post tasks to other thread and wait here
				if(checkState){
					while(!nextFrame.isAcquirable()){
						consumeOne();

						if(nextFrame.isNotTransferring()){
							nextFrame.activate(unitOffset);
							break;
						}
					}
				}

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
				currentFrame.endAcquire();
				{
					std::lock_guard lkq{drawQueueMutex};
					{
						std::shared_lock lk{imageUpdateMutex};
						pendingDrawCalls.emplace(&currentFrame, currentUsedImages);
					}
				}


			} else{
				semaphore_indexIncrement.release();
				acquirer.lock();
			}

			return nextFrame;
		}

		void recordTransferCommand(LockableVerticesData* frameData, CommandUnit& commandUnit) const{
			std::uint32_t count{};

			if(frameData){
				auto lk = frameData->captureUnique();
				count = frameData->getCount();
			}

			BasicBatch::recordTransferCommand(count, frameData, commandUnit);
		}
	};
}

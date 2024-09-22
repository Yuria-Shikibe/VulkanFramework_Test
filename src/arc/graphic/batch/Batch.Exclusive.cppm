module;

#include <vulkan/vulkan.h>

export module Graphic.Batch.Exclusive;

export import Graphic.Batch.Base;

import std;
import ext.cond_atomic;

namespace Graphic{
	class ExclusiveVerticesData : public UniversalVerticesData<false>{

	public:
		[[nodiscard]] ExclusiveVerticesData() = default;

		constexpr void undo(const std::size_t size) noexcept{
			count -= size;
		}
	};

	export struct Batch_Exclusive : BasicBatch<ExclusiveVerticesData>{
		using BasicBatch::BasicBatch;

		void consumeAll(){
			while(true){
				if(!consumeOne())break;
			}

			if(!currentVerticesData().empty()){
				toNextVerticesData(currentIndex);
				consumeOne();
			}
		}

		/**
		 * @brief multi call is allowed, do nothing when there is no frame valid
		 */
		bool consumeOne(){
			if(pendingDrawCalls.empty()){
				return false;
			}

			SubmittedDrawCall topDrawCall = pendingDrawCalls.front();
			pendingDrawCalls.pop();

			const auto index = units.get_index();
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

		[[nodiscard]] DrawArgs acquire(VkImageView imageView, const std::size_t count = 1){
			if(!imageView) throw std::invalid_argument("ImageView is null");

			ImageIndex imageIndex = getMappedImageIndex(imageView);

			while(imageIndex == InvalidImageIndex){
				toNextVerticesData(currentIndex);
				clearImageState();

				imageIndex = getMappedImageIndex(imageView);
			}

			auto [ptr, sz] = acquireValidSegments(count);
			return {imageIndex, static_cast<std::uint32_t>(sz), ptr};
		}


	private:
		std::tuple<std::byte*, std::size_t> acquireValidSegments(const std::size_t count){
			std::pair<std::byte*, std::size_t> rst{};

			while(true){
				auto& frame = currentVerticesData();

				rst = frame.acquire(count, unitOffset);

				if(rst.second == count){
					return rst;
				}

				frame.undo(rst.second);
				toNextVerticesData(currentIndex);
			}
		}

		ExclusiveVerticesData& toNextVerticesData(const std::size_t currentFrameIndex){
			const std::size_t nextFrameIndex = (currentFrameIndex + 1) % BufferSize;
			ExclusiveVerticesData& currentFrame = frames[currentFrameIndex];
			ExclusiveVerticesData& nextFrame = frames[nextFrameIndex];

			//Consume if overflow
			//TODO post tasks to other thread and wait here
			while(!nextFrame.isAcquirable()){
				consumeOne();

				if(nextFrame.isNotTransferring()){
					nextFrame.activate(unitOffset);
					break;
				}
			}

			currentFrame.endAcquire();
			currentIndex = nextFrameIndex;

			pendingDrawCalls.emplace(&currentFrame, currentUsedImages);

			return nextFrame;
		}

		void recordTransferCommand(ExclusiveVerticesData* frameData, CommandUnit& commandUnit) const{
			BasicBatch::recordTransferCommand(frameData->getCount(), frameData, commandUnit);
		}
	};
	//TODO single (submit) thread batch
}

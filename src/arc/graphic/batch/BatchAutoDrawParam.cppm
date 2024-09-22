module;

#include <vulkan/vulkan.h>

export module Graphic.Batch.AutoDrawParam;

export import Graphic.Batch;
export import Graphic.Batch.Exclusive;
export import Graphic.Draw.Interface;
export import Graphic.BatchData;

import std;

namespace Graphic{
	template <typename ArgTy, typename Vertex, typename T, typename Dv>
	struct AcquirerImpl : Graphic::BasicAcquirer<AutoDrawSpaceAcquirer<ArgTy, Vertex, Dv>>, Draw::DrawParam<T>{
	public:
		using BasicAcquirer<AutoDrawSpaceAcquirer<ArgTy, Vertex, Dv>>::BasicAcquirer;

		AcquirerImpl& operator++(){
			std::tie(this->dataPtr, this->index.textureIndex) = this->next();
			return *this;
		}
	};

	export
	template <typename Vertex, typename T = std::identity>
	struct BatchAutoParam_MultiThread : AutoDrawSpaceAcquirer<LockableDrawArgs, Vertex, BatchAutoParam_MultiThread<Vertex, T>>{
		using Acquirer = AcquirerImpl<LockableDrawArgs, Vertex, T, BatchAutoParam_MultiThread>;

		Batch_MultiThread* batch{};
		const ImageViewRegion* region{};

		[[nodiscard]] BatchAutoParam_MultiThread() = default;

		[[nodiscard]] explicit BatchAutoParam_MultiThread(Batch_MultiThread& batch, const ImageViewRegion* region = nullptr)
			: batch{&batch}, region{region}{}

		void setRegion(const ImageViewRegion* region){
			this->region = region;
		}

		void reserve(const std::size_t count){
			this->append(batch->acquireOnce(region->imageView, count));
		}

		Acquirer get(){
			Acquirer acquirer{*this};
			acquirer.uv = region;
			return acquirer;
		}
	};


	export
	template <typename Vertex, typename T = std::identity>
	struct BatchAutoParam_Exclusive : AutoDrawSpaceAcquirer<DrawArgs, Vertex, BatchAutoParam_Exclusive<Vertex, T>>{
		using Acquirer = AcquirerImpl<DrawArgs, Vertex, T, BatchAutoParam_Exclusive>;

		Batch_Exclusive* batch{};
		const ImageViewRegion* region{};

		[[nodiscard]] BatchAutoParam_Exclusive() = default;

		[[nodiscard]] explicit BatchAutoParam_Exclusive(Batch_Exclusive& batch, const ImageViewRegion* region = nullptr)
			: batch{&batch}, region{region}{}

		void setRegion(const ImageViewRegion* region){
			this->region = region;
		}

		void reserve(const std::size_t count){
			this->append(batch->acquire(region->imageView, count));
		}

		Acquirer get(){
			Acquirer acquirer{*this};
			acquirer.uv = region;
			return acquirer;
		}
	};

	export
	template <typename Vertex /*capture name*/ = void, typename T = std::identity>
	struct InstantBatchAutoParam : Draw::DrawParam<T>{
		Batch_Exclusive* batch{};
		VkImageView imageView{};

		[[nodiscard]] InstantBatchAutoParam() = default;

		[[nodiscard]] InstantBatchAutoParam(Batch_Exclusive& batch, const ImageViewRegion* region)
			: batch{&batch},
			  imageView{region->imageView}{
			this->uv = region;
		}

		InstantBatchAutoParam& operator++(){
			auto param = batch->acquire(imageView);
			this->dataPtr = param.dataPtr;
			this->index.textureIndex = param.imageIndex;
			return *this;
		}

		void setImage(const ImageViewRegion* region){
			imageView = region->imageView;
			this->uv = region;
		}
	};

	// struct T{
	// 	struct iterator{
	//
	// 	};
	// 	int *p{};
	//
	// 	iterator begin(){
	// 		return {};
	// 	}
	// };

	// void foo(){
	// 	BatchAutoParam<int> v{};
	// 	std::ranges::begin(v);
	// 	T t{};
	// 	std::ranges::begin(t);
	// }
}

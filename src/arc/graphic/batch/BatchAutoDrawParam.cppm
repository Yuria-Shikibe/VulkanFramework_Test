module;

#include <vulkan/vulkan.h>
#include <cassert>

export module Graphic.Batch.AutoDrawParam;

export import Graphic.Batch.MultiThread;
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
	struct BatchAutoParam_MultiThread : Draw::AutoParamBase<Vertex, T>, AutoDrawSpaceAcquirer<LockableDrawArgs, Vertex, BatchAutoParam_MultiThread<Vertex, T>>{
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
			this->append(batch->acquireOnce(region->view, count));
		}

		Acquirer get(){
			Acquirer acquirer{*this};
			acquirer.uv = region;
			return acquirer;
		}
	};


	export
	template <typename Vertex /*capture name*/ = void, typename T = std::identity>
	class InstantBatchAutoParam : public Draw::AutoParamBase<Vertex, T>{
		Batch_Exclusive* batch{};
		VkImageView imageView{};

	public:
		[[nodiscard]] InstantBatchAutoParam() = default;

		[[nodiscard]] InstantBatchAutoParam(Batch_Exclusive& batch, const ImageViewRegion* region)
			: batch{&batch},
			  imageView{region->view}{
			this->uv = region;
		}

		[[nodiscard]] InstantBatchAutoParam(Batch_Exclusive& batch)
			: batch{&batch}{}

		InstantBatchAutoParam& operator++(){
			auto param = batch->acquire(imageView);
			this->dataPtr = param.dataPtr;
			this->index.textureIndex = param.imageIndex;
			return *this;
		}

		void setImage(const ImageViewRegion* region){
			imageView = region->view;
			this->uv = region;
		}

		InstantBatchAutoParam& operator << (VkImageView view){
			imageView = view;
			return *this;
		}

		InstantBatchAutoParam& operator << (const UVData& uv){
			this->uv = &uv;
			return *this;
		}

		InstantBatchAutoParam& operator << (const UVData* uv){
			assert(uv != nullptr);

			return this->operator<<(*uv);
		}

		InstantBatchAutoParam& operator << (const ImageViewRegion& region){
			return this->operator<<(region.view) << static_cast<const UVData&>(region);
		}

		InstantBatchAutoParam& operator << (const ImageViewRegion* region){
			assert(region != nullptr);

			return this->operator<<(*region);

		}
	};

	export
	template <typename Vertex = int, Draw::VertexModifier<Vertex> T = std::identity>
	Draw::DrawParam<T> getParamOf(Batch_Exclusive& batch, const ImageViewRegion& region, T modifier = {}){
		auto rst = batch.acquire(region.view, 1);
		return Draw::DrawParam<T>{
			.dataPtr = rst.dataPtr,
			.index = TextureIndex{rst.imageIndex},
			.uv = &region,
			.modifier = std::move(modifier),
		};
	}

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

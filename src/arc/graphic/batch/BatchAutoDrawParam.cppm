export module Graphic.Batch.AutoDrawParam;

export import Graphic.Batch;
export import Graphic.Draw.Interface;
export import Core.Vulkan.BatchData;

import std;

namespace Graphic{
	template <typename Vertex, typename T, typename Dv>
	struct AcquirerImpl : Graphic::BasicAcquirer<AutoDrawSpaceAcquirer<Vertex, Dv>>, Draw::DrawParam<T>{
	public:
		using BasicAcquirer<AutoDrawSpaceAcquirer<Vertex, Dv>>::BasicAcquirer;

		AcquirerImpl& operator++(){
			std::tie(this->dataPtr, this->index.textureIndex) = this->next();
			return *this;
		}
	};

 	export
	template <typename Vertex, typename T = std::identity>
	struct BatchAutoParam : AutoDrawSpaceAcquirer<Vertex, BatchAutoParam<Vertex, T>>{
 		using Acquirer = AcquirerImpl<Vertex, T, BatchAutoParam>;

		Batch* batch{};
		const ImageViewRegion* region{};

	    [[nodiscard]] BatchAutoParam() = default;

	    [[nodiscard]] explicit BatchAutoParam(Batch& batch, const ImageViewRegion* region = nullptr)
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
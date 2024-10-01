// ReSharper disable CppDFANotInitializedField
module;


#include "../src/ext/assume.hpp"

export module Graphic.ImageNineRegion;

export import Graphic.ImageRegion;
import Geom.Vector2D;
import Geom.GridGenerator;
import Geom.Rect_Orthogonal;
import Align;

import std;
import ext.meta_programming;

namespace Graphic{
	constexpr Align::Scale DefaultScale = Align::Scale::stretch;


	template <typename T = float>
		requires (std::is_arithmetic_v<T>)
	using Generator = Geom::grid_generator<4, T>;

	using NinePatchProp = Geom::grid_property<4>;
	constexpr auto NinePatchSize = NinePatchProp::size;

	export
	template <typename T = float>
		requires (std::is_arithmetic_v<T>)
	struct NinePatchRaw{
		using Rect = Geom::Rect_Orthogonal<T>;

		std::array<Rect, NinePatchSize> values{};

		[[nodiscard]] constexpr NinePatchRaw() noexcept = default;

		[[nodiscard]] constexpr NinePatchRaw(const Rect internal, const Rect external) noexcept
			: values{
				Geom::createGrid<4, T>({
						external.vert_00(),
						internal.vert_00(),
						internal.vert_11(),
						external.vert_11()
					})
			}{}

		[[nodiscard]] constexpr NinePatchRaw(const Align::Pad<T> edge, const Rect external) noexcept
			: values{
				Geom::createGrid<4, T>({
						external.vert_00(),
						external.vert_00() + edge.bot_lft(),
						external.vert_11() - edge.top_rit(),
						external.vert_11()
					})
			}{}


		[[nodiscard]] constexpr NinePatchRaw(
			const Align::Spacing edge,
			const Rect rect,
			const Geom::Vector2D<T> centerSize,
			const Align::Scale centerScale) noexcept
			: NinePatchRaw{edge, rect}{

			NinePatchRaw::setCenterScale(centerSize, centerScale);
		}


		[[nodiscard]] constexpr NinePatchRaw(
			const Rect internal, const Rect external,
			const Geom::Vector2D<T> centerSize,
			const Align::Scale centerScale) noexcept
			: NinePatchRaw{internal, external}{

			NinePatchRaw::setCenterScale(centerSize, centerScale);
		}

		constexpr void setCenterScale(const Geom::Vector2D<T> centerSize, const Align::Scale centerScale) noexcept{
			if(centerScale != DefaultScale){
				const auto sz = Align::embedTo(centerScale, centerSize, center().getSize());
				const auto offset = Align::getOffsetOf<ext::to_signed_t<T>>(Align::Pos::center, sz.asSigned(), center().asSigned());
				center() = {Geom::FromExtent, static_cast<Geom::Vector2D<T>>(offset), sz};
			}
		}

		constexpr Rect operator [](const unsigned index) const noexcept{
			return values[index];
		}

		[[nodiscard]] constexpr decltype(auto) center() noexcept{
			return values[Geom::grid_property<4>::center_index];
		}

		[[nodiscard]] constexpr decltype(auto) center() const noexcept{
			return values[Geom::grid_property<4>::center_index];
		}
	};

	export
	struct NinePatchBrief{
		Align::Spacing edge{};

		Geom::Vec2 innerSize{};
		Align::Scale centerScale{DefaultScale};

		[[nodiscard]] constexpr NinePatchRaw<float> getPatches(const NinePatchRaw<float>::Rect bound) const noexcept{
			return NinePatchRaw{edge, bound, innerSize, centerScale};
		}
	};

	export
	struct ImageViewNineRegion : NinePatchBrief{
		static constexpr auto size = NinePatchSize;
		SizedImageView imageView{};
		std::array<UVData, NinePatchSize> regions{};

		[[nodiscard]] ImageViewNineRegion() = default;

		ImageViewNineRegion(
			const ImageViewRegion& imageRegion,
			Geom::OrthoRectUInt internal_in_relative,
			const Geom::USize2 centerSize = {},
			const Align::Scale centerScale = DefaultScale,
			const float edgeShrinkScale = 0.25f
		){

			auto external = imageRegion.getBound();
			internal_in_relative.src += external.src;

			CHECKED_ASSUME(external.containsLoose(internal_in_relative));

			imageView = static_cast<SizedImageView>(imageRegion);

			const auto ninePatch = NinePatchRaw{internal_in_relative, external, centerSize, centerScale};

			for (auto && [i, region] : regions | std::views::enumerate){
				region.setUV_fromInternal(imageView.size, ninePatch[i]);
			}


			this->centerScale = centerScale;
			this->innerSize = centerSize.as<float>();
			this->edge = Align::padBetween(internal_in_relative.as<float>(), external.as<float>());

			using gen = Generator<float>;

			for (const auto hori_edge_index : gen::property::edge_indices | std::views::take(gen::property::edge_indices.size() / 2)){
				regions[hori_edge_index].shrinkEdge(imageView.size, {edgeShrinkScale * innerSize.x, 0});
			}

			for (const auto vert_edge_index : gen::property::edge_indices | std::views::drop(gen::property::edge_indices.size() / 2)){
				regions[vert_edge_index].shrinkEdge(imageView.size, {0, edgeShrinkScale * innerSize.y});
			}

			for (auto && uvData : regions){
				uvData.flipY();
			}

		}

		ImageViewNineRegion(
			const ImageViewRegion& imageRegion,
			const Align::Pad<std::uint32_t> margin,
			const Geom::USize2 centerSize = {},
			const Align::Scale centerScale = Align::Scale::fill,
			const float edgeShrinkScale = 0.25f
		) : ImageViewNineRegion{imageRegion, Geom::OrthoRectUInt{
			Geom::FromExtent, margin.bot_lft(), imageRegion.getBound().getSize().sub(margin.getSize())
		}, centerSize, centerScale, edgeShrinkScale}{
			assert(margin.getSize().within(imageRegion.getBound().getSize()));
		}

		ImageViewNineRegion(
			const ImageViewRegion& external,
			const Align::Pad<std::uint32_t> margin,
			const ImageViewRegion& internal,
			const Align::Scale centerScale = Align::Scale::fill,
			const float edgeShrinkScale = 0.25f
		) : ImageViewNineRegion{
				external,
				margin,
				internal.getSize(),
				centerScale,
				edgeShrinkScale
			}{

			assert(static_cast<const SizedImageView&>(external) == static_cast<const SizedImageView&>(internal));

			regions[NinePatchProp::center_index] = UVData(internal);
		}


		ImageViewNineRegion(
			const ImageViewRegion& external,
			const Align::Pad<std::uint32_t> margin,
			const ImageViewRegion& internal,
			const Geom::USize2 centerSize,
			const Align::Scale centerScale = Align::Scale::fill,
			const float edgeShrinkScale = 0.25f
		) : ImageViewNineRegion{
				external,
				margin,
				centerSize,
				centerScale,
				edgeShrinkScale,
			}{

			assert(static_cast<const SizedImageView&>(external) == static_cast<const SizedImageView&>(internal));

			regions[NinePatchProp::center_index] = UVData(internal);
		}


	};
}

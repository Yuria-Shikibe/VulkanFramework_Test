//
// Created by Matrix on 2024/5/31.
//

export module Geom.QuadTree.Interface;

export import Geom.Vector2D;
export import Geom.Rect_Orthogonal;
import ext.concepts;

export namespace Geom{

	template <typename T, ext::number N = float>
	struct QuadTreeAdaptable{
		using coordinate_type = N;

		[[nodiscard]] Rect_Orthogonal<N> getBound() const noexcept = delete;

		[[nodiscard]] bool roughIntersectWith(const T& other) const = delete;

		[[nodiscard]] bool exactIntersectWith(const T& other) const = delete;

		[[nodiscard]] bool containsPoint(typename Vector2D<N>::PassType point) const = delete;
	};
}

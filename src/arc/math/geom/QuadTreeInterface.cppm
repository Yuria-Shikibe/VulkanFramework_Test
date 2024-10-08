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
		Rect_Orthogonal<N> getBound() const noexcept = delete;

		bool roughIntersectWith(const T& other) const = delete;

		bool exactIntersectWith(const T& other) const = delete;

		bool containsPoint(typename Vector2D<N>::PassType point) const = delete;
	};
}

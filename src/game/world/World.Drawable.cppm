//
// Created by Matrix on 2024/10/18.
//

export module Game.World.Drawable;

export import Geom.Rect_Orthogonal;

export namespace Game{
	// template <typename T>
	struct Drawable{
		virtual ~Drawable() = default;

		virtual void draw() const = 0;

		[[nodiscard]] virtual Geom::OrthoRectFloat getClipRegion() const noexcept = 0;
	};
}

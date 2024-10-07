//
// Created by Matrix on 2024/4/14.
//

export module Core.UI.RegionDrawable;

export import Geom.Rect_Orthogonal;
export import Geom.Vector2D;
export import Graphic.Color;
export import Graphic.ImageRegion;
export import Graphic.ImageNineRegion;

import std;

namespace Graphic{
	export struct RendererUI;
}

export namespace Core::UI{
	struct RegionDrawable {
		virtual ~RegionDrawable() = default;

		virtual void draw(Graphic::RendererUI& renderer, Geom::OrthoRectFloat rect, Graphic::Color color) const{

		}

		[[nodiscard]] virtual Geom::Vec2 getDefSize() const{
			return {};
		}
	};

	struct ImageRegionDrawable : RegionDrawable{
		Graphic::ImageViewRegion region{};

		[[nodiscard]] constexpr ImageRegionDrawable() = default;

		[[nodiscard]] explicit constexpr ImageRegionDrawable(const Graphic::ImageViewRegion& region)
			: region{region}{}

		void draw(Graphic::RendererUI& renderer, Geom::OrthoRectFloat rect, Graphic::Color color) const override;

		[[nodiscard]] Geom::Vec2 getDefSize() const override{
			return region.getSize().as<float>();
		}
	};

	using Icon = ImageRegionDrawable;

	struct TextureNineRegionDrawable_Ref : RegionDrawable{
		Graphic::ImageViewNineRegion* region{nullptr};

		[[nodiscard]] explicit TextureNineRegionDrawable_Ref(Graphic::ImageViewNineRegion* const rect)
			: region(rect) {
		}

		void draw(Graphic::RendererUI& renderer, Geom::OrthoRectFloat rect, Graphic::Color color) const override;

		[[nodiscard]] Geom::Vec2 getDefSize() const override{
			return region->getRecommendedSize();
		}
	};

	struct TextureNineRegionDrawable_Owner : RegionDrawable{
		Graphic::ImageViewNineRegion region{};

		[[nodiscard]] TextureNineRegionDrawable_Owner() = default;

		[[nodiscard]] explicit TextureNineRegionDrawable_Owner(const Graphic::ImageViewNineRegion& rect)
			: region(rect) {
		}

		void draw(Graphic::RendererUI& renderer, Geom::OrthoRectFloat rect, Graphic::Color color) const override;

		[[nodiscard]] Geom::Vec2 getDefSize() const override{
			return region.getRecommendedSize();
		}
	};


	// struct UniqueRegionDrawable : RegionDrawable{
	// 	std::unique_ptr<GL::Texture2D> texture{};
	// 	GL::TextureRegion wrapper{};
	//
	// 	template <typename ...T>
	// 	explicit UniqueRegionDrawable(T&& ...args) : texture{std::make_unique<GL::Texture2D>(std::forward<T>(args)...)}, wrapper{texture.get()}{}
	//
	// 	explicit UniqueRegionDrawable(std::unique_ptr<GL::Texture2D>&& texture) : texture{std::move(texture)}, wrapper{texture.get()}{}
	//
	// 	explicit UniqueRegionDrawable(GL::Texture2D&& texture) : texture{std::make_unique<GL::Texture2D>(std::move(texture))}, wrapper{this->texture.get()}{}
	//
	// 	void draw(const Geom::OrthoRectFloat rect) const override;
	//
	// 	Geom::Vec2 getDefSize() const override{
	// 		return wrapper.getSize();
	// 	}
	// };
}

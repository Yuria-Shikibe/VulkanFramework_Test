//
// Created by Matrix on 2024/10/7.
//

export module Core.UI.Styles;

export import Core.UI.Drawer;

import Graphic.ImageRegion;
import Graphic.ImageNineRegion;
export import Graphic.Color;

import std;

export namespace Core::UI{
	struct Element;

	namespace Style{
		struct Palette{
			Graphic::Color general{};
			Graphic::Color onFocus{};
			Graphic::Color onPress{};

			Graphic::Color disabled{};
			Graphic::Color activated{};

			constexpr Palette& mulAlpha(const float alpha) noexcept{
				general.mulA(alpha);
				onFocus.mulA(alpha);
				onPress.mulA(alpha);

				disabled.mulA(alpha);
				activated.mulA(alpha);

				return *this;
			}

			[[nodiscard]] Graphic::Color onInstance(const Element& element) const;
		};

		template <typename T>
		struct PaletteWith : public T{
			Palette palette{};

			[[nodiscard]] PaletteWith() = default;

			[[nodiscard]] explicit PaletteWith(const T& val, const Palette& palette)
				requires (std::is_copy_constructible_v<T>)
				: T{val}, palette{palette}{}
		};

		struct RoundStyle : StyleDrawer<struct Element>{
			PaletteWith<Graphic::ImageViewNineRegion> base{};
			PaletteWith<Graphic::ImageViewNineRegion> edge{};

			void draw(const Element& element) const override;
		};
	}
}

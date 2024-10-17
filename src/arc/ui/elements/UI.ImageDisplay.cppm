//
// Created by Matrix on 2024/9/24.
//

export module Core.UI.ImageDisplay;

export import Core.UI.Element;
export import Core.UI.RegionDrawable;

import std;

namespace Core::UI{
	//TODO ImageDisplay with static type

	struct ImageDisplayBase : public Element{
		Align::Scale scaling{Align::Scale::fit};
		Align::Pos imageAlign{Align::Pos::center};

		[[nodiscard]] explicit ImageDisplayBase(
			const Align::Scale scaling,
			const Align::Pos imageAlign = Align::Pos::center)
			: scaling{scaling},
			  imageAlign{imageAlign}{}

		[[nodiscard]] explicit ImageDisplayBase(
			const std::string_view tyName,
			const Align::Scale scaling = Align::Scale::fit,
			const Align::Pos imageAlign = Align::Pos::center)
			: Element{tyName},
			  scaling{scaling},
			  imageAlign{imageAlign}{}

		[[nodiscard]] ImageDisplayBase() = default;
	};

	export
	struct ImageDisplay : public ImageDisplayBase{
	private:
		std::unique_ptr<RegionDrawable> drawable{};
		std::move_only_function<decltype(drawable)(ImageDisplay&)> drawableProv{};

	public:
		[[nodiscard]] ImageDisplay() : ImageDisplayBase{"ImageDisplay"}{}

		template <typename Drawable>
			requires std::derived_from<std::decay_t<Drawable>, RegionDrawable>
		[[nodiscard]] explicit ImageDisplay(
			Drawable&& drawable,
			const Align::Scale scaling = Align::Scale::fit,
			const Align::Pos imageAlign = Align::Pos::center/*, const bool useEmptyDrawer = true*/)
			:
			ImageDisplayBase{"ImageDisplay", scaling, imageAlign},
			drawable{std::make_unique<std::decay_t<Drawable>>(std::forward<Drawable>(drawable))}{}

		[[nodiscard]] bool isDynamic() const noexcept{
			return drawableProv != nullptr;
		}

		template <typename Fn>
		void setProvider(Fn&& fn){
			Util::setFunction(drawableProv, std::forward<Fn>(fn));
		}

		void setDrawable(std::unique_ptr<RegionDrawable>&& drawable){
			this->drawable = std::move(drawable);
		}

		template <typename Drawable>
			requires std::derived_from<std::decay_t<Drawable>, RegionDrawable>
		void setDrawable(Drawable&& drawable){
			this->drawable = std::make_unique<std::decay_t<Drawable>>(std::forward<Drawable>(drawable));
		}

		void update(const float delta_in_tick) override{
			Element::update(delta_in_tick);

			if(isDynamic()){
				drawable = drawableProv(*this);
			}
		}

		void drawMain() const override;
	};

	export
	template <std::derived_from<RegionDrawable> Drawable>
	struct FixedImageDisplay : public ImageDisplayBase{
	private:
		Drawable drawable;
		std::move_only_function<decltype(drawable)(FixedImageDisplay&)> drawableProv{};

	public:
		[[nodiscard]] FixedImageDisplay() requires(std::is_default_constructible_v<Drawable>)
			: ImageDisplayBase{"ImageDisplay"}, drawable{}{}

		template <std::convertible_to<Drawable> Region>
		[[nodiscard]] explicit FixedImageDisplay(
			Region&& drawable,
			const Align::Scale scaling = Align::Scale::fit,
			const Align::Pos imageAlign = Align::Pos::center/*, const bool useEmptyDrawer = true*/)
			:
			ImageDisplayBase{"ImageDisplay", scaling, imageAlign},
			drawable{std::forward<Region>(drawable)}{

		}

		[[nodiscard]] bool isDynamic() const noexcept{
			return drawableProv != nullptr;
		}

		template <typename Fn>
		void setProvider(Fn&& fn){
			Util::setFunction(drawableProv, std::forward<Fn>(fn));
		}

		template <std::convertible_to<Drawable> Region>
		void setDrawable(Region&& drawable){
			this->drawable = std::forward<Region>(drawable);
		}

		void update(const float delta_in_tick) override{
			Element::update(delta_in_tick);

			if(isDynamic()){
				drawable = drawableProv(*this);
			}
		}

		void drawMain() const override{
			ImageDisplayBase::drawMain();
			// auto& region = drawable;//static_cast<const RegionDrawable&>(drawable);

			const auto size = Align::embedTo(scaling, drawable.getDefSize(), getValidSize());
			const auto offset = Align::getOffsetOf(imageAlign, size, property.getValidBound_absolute());
			drawable.draw(getRenderer(), Rect{Geom::FromExtent, offset, size}, graphicProp().getScaledColor());
		}
	};
}

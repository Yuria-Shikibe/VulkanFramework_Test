//
// Created by Matrix on 2024/9/24.
//

export module Core.UI.ImageDisplay;

export import Core.UI.Element;
export import Core.UI.RegionDrawable;

import std;

export namespace Core::UI{
	struct ImageDisplay : public Element{
		std::unique_ptr<RegionDrawable> drawable{};
		std::move_only_function<decltype(drawable)(ImageDisplay&)> drawableProv{};

		Align::Scale scaling{Align::Scale::fit};
		Align::Pos imageAlign{Align::Pos::center};

		[[nodiscard]] ImageDisplay() : Element{"ImageDisplay"}{}

		template <typename Drawable>
			requires std::derived_from<std::decay_t<Drawable>, RegionDrawable>
		[[nodiscard]] explicit ImageDisplay(
			Drawable&& drawable,
			const Align::Scale scale = Align::Scale::fit/*, const bool useEmptyDrawer = true*/)
			:
			Element{"ImageDisplay"},
			drawable{std::make_unique<std::decay_t<Drawable>>(std::forward<Drawable>(drawable))},
			scaling{scale}{}

		[[nodiscard]] bool isDynamic() const noexcept{
			return drawableProv != nullptr;
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
}

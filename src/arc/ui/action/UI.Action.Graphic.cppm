export module Core.UI.Action.Graphic;

import Core.UI.Element;
export import Core.UI.Action;
import Graphic.Color;

export namespace Core::UI::Actions{
	struct ColorAction final : Action<Element>{
	private:
		Graphic::Color initialColor{};

	public:
		Graphic::Color endColor{};

		ColorAction(const float lifetime, const Graphic::Color& endColor)
			: Action<Element>(lifetime),
			  endColor(endColor){}

		ColorAction(const float lifetime, const Graphic::Color& endColor, const Math::Interp::InterpFunc& interpFunc)
			: Action<Element>(lifetime, interpFunc),
			  endColor(endColor){}

		void apply(Element& elem, const float progress) override{
			elem.prop().graphicData.baseColor = initialColor.createLerp(endColor, progress);
		}

		void begin(Element& elem) override{
			initialColor = elem.prop().graphicData.baseColor;
		}

		void end(Element& elem) override{
			elem.prop().graphicData.baseColor = endColor;
		}
	};

	struct AlphaAction : Action<Element>{
	protected:
		float initialAlpha{};

	public:
		float endAlpha{};

		AlphaAction(const float lifetime, const float endAlpha, const Math::Interp::InterpFunc& interpFunc)
			: Action<Element>(lifetime, interpFunc),
			  endAlpha(endAlpha){}

		AlphaAction(const float lifetime, const float endAlpha)
			: Action<Element>(lifetime),
			  endAlpha(endAlpha){}

		void apply(Element& elem, const float progress) override{
			elem.updateOpacity(Math::lerp(initialAlpha, endAlpha, progress));
		}

		void begin(Element& elem) override{
			initialAlpha = elem.graphicProp().inherentOpacity;
		}

		void end(Element& elem) override{
			elem.updateOpacity(endAlpha);
		}
	};

	struct RemoveAction : Action<Element>{
		RemoveAction() = default;

		void begin(Element& elem) override{
			elem.removeSelfFromParent();
		}
	};
}


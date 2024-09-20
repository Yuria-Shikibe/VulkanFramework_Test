//
// Created by Matrix on 2024/9/11.
//

export module Graphic.Effect.Derives;

export import Graphic.Effect;

import std;

export namespace Graphic{
	template <auto DrawFunc>
		requires (std::regular_invocable<Effect&, decltype(DrawFunc)>)
	struct EffectDrawer_StaticFunc final : EffectDrawer{
		explicit EffectDrawer_StaticFunc(const float time)
			: EffectDrawer(time){}

		void operator()(Effect& effect) const override{
			DrawFunc(effect);
		}
	};
}
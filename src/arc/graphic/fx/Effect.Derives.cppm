//
// Created by Matrix on 2024/9/11.
//

export module Graphic.Effect.Derives;

export import Graphic.Effect;

import std;

export namespace Graphic{
	template <auto DrawFunc, auto ClipRectFunc = nullptr>
		requires requires{
			requires std::regular_invocable<decltype(DrawFunc), Effect&>;
			requires
				std::same_as<decltype(ClipRectFunc), std::nullptr_t> ||
				std::is_nothrow_invocable_r_v<Geom::OrthoRectFloat, decltype(ClipRectFunc), const Effect&>;
		}
	struct EffectDrawer_StaticFunc final : EffectDrawer{
		using EffectDrawer::EffectDrawer;

		[[nodiscard]] constexpr explicit EffectDrawer_StaticFunc(const float defaultLifetime) requires (std::same_as<decltype(ClipRectFunc), std::nullptr_t>)
			: EffectDrawer(defaultLifetime){
		}

		[[nodiscard]] constexpr EffectDrawer_StaticFunc(const float defLifetime, const float defClipRadius) requires (std::same_as<decltype(ClipRectFunc), std::nullptr_t>)
			: EffectDrawer(defLifetime, defClipRadius){
		}

		[[nodiscard]] constexpr explicit EffectDrawer_StaticFunc(const float defaultLifetime) requires (!std::same_as<decltype(ClipRectFunc), std::nullptr_t>)
			: EffectDrawer(defaultLifetime, std::numeric_limits<float>::signaling_NaN()){
		}

		void operator()(Effect& effect) const override{
			return std::invoke(DrawFunc, effect);
		}

		Geom::OrthoRectFloat getClipBoundVirtual(const Effect& effect) const noexcept override {
			if constexpr (!std::same_as<decltype(ClipRectFunc), std::nullptr_t>){
				return std::invoke_r<Geom::OrthoRectFloat>(ClipRectFunc, effect);
			}else{
				return EffectDrawer::getClipBoundVirtual(effect);
			}
		}
	};
}
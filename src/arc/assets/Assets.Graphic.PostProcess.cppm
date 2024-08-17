//
// Created by Matrix on 2024/8/17.
//

export module Assets.Graphic.PostProcess;

import Graphic.PostProcessor;
import Geom.Vector2D;
// import std;

export namespace Assets{
	namespace PostProcess{
		struct BlurConstants_Step{
			static constexpr float UnitFar = 3.2307692308f;
			static constexpr float UnitNear = 1.3846153846f;

			Geom::Vec2 stepNear{};
			Geom::Vec2 stepFar{};
		};

		struct BlurConstants_Scl{
			float center = 0.22970270270f;
			float close = 0.33062162162f;
			float far = 0.0822702703f;
		};

		namespace Factory{
			Graphic::PostProcessorFactory blurProcessorFactory{};
		}

		void load(const Core::Vulkan::Context& context);

		void dispose();
	}
}
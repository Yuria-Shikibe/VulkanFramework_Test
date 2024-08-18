//
// Created by Matrix on 2024/8/17.
//

export module Assets.Graphic.PostProcess;

import Graphic.PostProcessor;
import Geom.Vector2D;
// import std;

export namespace Assets{
	namespace PostProcess{
		struct BlurConstants{
			static constexpr float UnitFar = 3.2307692308f;
			static constexpr float UnitNear = 1.3846153846f;

			Geom::Vec2 stepNear{};
			Geom::Vec2 stepFar{};

			float center = 0.23270270270f;
			float close = 0.32062162162f;
			float far = 0.0822702703f;
		};

		namespace Factory{
			Graphic::PostProcessorFactory blurProcessorFactory{};
			Graphic::PostProcessorFactory mergeBloomFactory{};
		}

		void load(const Core::Vulkan::Context& context);

		void dispose();
	}
}
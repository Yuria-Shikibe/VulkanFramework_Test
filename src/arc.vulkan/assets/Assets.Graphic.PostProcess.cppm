//
// Created by Matrix on 2024/8/17.
//

export module Assets.Graphic.PostProcess;

import Graphic.PostProcessor;
import Geom.Vector2D;
import std;

export namespace Assets{
	namespace PostProcess{
		struct BlurConstants{
			static constexpr float UnitFar = 3.2307692308f;
			static constexpr float UnitNear = 1.3846153846f;

			Geom::Vec2 stepNear{};
			Geom::Vec2 stepFar{};

			float center = 0.25170270270f;
			float close = 0.32362162162f;
			float far = 0.0822702703f;
		};

	    struct UniformBlock_SSAO {
	        struct SSAO_KernalInfo {
	            std::size_t count{};
	            float distance{};
	            float weight{};
	        };


            static constexpr std::array SSAO_Params{
                    SSAO_KernalInfo{20, 0.15702702700f},
                    SSAO_KernalInfo{16, 0.32702702700f},
                    SSAO_KernalInfo{16, 0.55062162162f},
                    SSAO_KernalInfo{12, 0.83062162162f},
                };

	        static constexpr std::size_t DefKernalSize{[]{
	            std::size_t rst{};

	            for (const auto & ssao_param : SSAO_Params) {
	                rst += ssao_param.count;
	            }

	            return rst;
	        }()};

	        static constexpr std::size_t KernalMaxSize = 64;

	        static_assert(DefKernalSize <= KernalMaxSize);

	        std::array<std::pair<Geom::Vec2, Geom::Vec2>, KernalMaxSize> kernal{};
	        std::uint32_t kernalSize{DefKernalSize};
	    };

		namespace Factory{
			Graphic::PostProcessorFactory blurProcessorFactory{};
			Graphic::PostProcessorFactory mergeBloomFactory{};
			Graphic::PostProcessorFactory nfaaFactory{};
		}

		void load(const Core::Vulkan::Context& context);

		void dispose();
	}
}
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
                    SSAO_KernalInfo{16, 0.15702702700f},
                    SSAO_KernalInfo{8, 0.32702702700f},
                    SSAO_KernalInfo{8, 0.55062162162f},
                    SSAO_KernalInfo{4, 0.83062162162f},
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

		struct Gaussian_KernalInfo {
			struct Off{
				Geom::Vec2 off{};
				float weight{};
				float cap{};
			};
			std::array<Off, 3> offs{};
			float srcWeight{};
		};

		namespace Factory{
			Graphic::GraphicPostProcessorFactory blurProcessorFactory{};
			Graphic::GraphicPostProcessorFactory mergeBloomFactory{};
			Graphic::GraphicPostProcessorFactory game_uiMerge{};
			Graphic::GraphicPostProcessorFactory nfaaFactory{};

			Graphic::ComputePostProcessorFactory gaussianFactory{};
		}

		void load(const Core::Vulkan::Context& context);

		void dispose();
	}
}
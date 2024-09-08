//
// Created by Matrix on 2024/8/17.
//

export module Assets.Graphic.PostProcess;

import Graphic.PostProcessor;
import Geom.Vector2D;
import std;

namespace Assets{
	namespace PostProcess{
		export struct BlurConstants{
			static constexpr float UnitFar = 3.2307692308f;
			static constexpr float UnitNear = 1.3846153846f;

			Geom::Vec2 stepNear{};
			Geom::Vec2 stepFar{};

			float center = 0.25170270270f;
			float close = 0.32362162162f;
			float far = 0.0822702703f;
		};

	    export struct UniformBlock_kernalSSAO {
	    	struct T{
	    		Geom::Vec2 off{};
	    		float weight{};
	    		float cap{};
	    	};

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

	        [[nodiscard]] constexpr UniformBlock_kernalSSAO() = default;

	        [[nodiscard]] constexpr explicit UniformBlock_kernalSSAO(const Geom::USize2 size){
	        	const auto scale = ~size.as<float>();

	        	for(std::size_t count{}; const auto& param : SSAO_Params){
	        		for(std::size_t i = 0; i < param.count; ++i){
	        			kernal[count].off.setPolar(
							(360.f / static_cast<float>(param.count)) * static_cast<float>(i), param.distance) *= scale;
	        			kernal[count].weight = param.weight;
	        			count++;
	        		}
	        	}
	        }

	        static constexpr std::size_t KernalMaxSize = 64;

	        static_assert(DefKernalSize <= KernalMaxSize);

	        std::array<T, KernalMaxSize> kernal{};
	        std::uint32_t kernalSize{DefKernalSize};

	        constexpr UniformBlock_kernalSSAO(const UniformBlock_kernalSSAO& other) = default;

	        constexpr UniformBlock_kernalSSAO(UniformBlock_kernalSSAO&& other) noexcept = default;

	        constexpr UniformBlock_kernalSSAO& operator=(const UniformBlock_kernalSSAO& other) = default;

	        constexpr UniformBlock_kernalSSAO& operator=(UniformBlock_kernalSSAO&& other) noexcept = default;
	    };


		//OPTM uses constant instead of uniform?
		export struct UniformBlock_kernalGaussian {
			struct Off{
				Geom::Vec2 off{};
				float weight{};
				float cap{};
			};
			std::array<Off, 3> offs{};
			float srcWeight{};
		};

		constexpr float Scl = 0.9f;

		export constexpr UniformBlock_kernalGaussian GaussianKernalVert{
			.offs = {
				UniformBlock_kernalGaussian::Off{Geom::Vec2{0.f, 1.5846153846f}, 0.29362162162f * Scl},
				UniformBlock_kernalGaussian::Off{Geom::Vec2{0.f, 3.3307692308f}, 0.11227027030f * Scl},
				UniformBlock_kernalGaussian::Off{Geom::Vec2{0.f, 5.2307692308f}, 0.06227027030f * Scl}
			},
			.srcWeight = 0.24170270270f * Scl
		};

		export constexpr UniformBlock_kernalGaussian GaussianKernalHori{
			[]{
				UniformBlock_kernalGaussian k = GaussianKernalVert;
				for(auto&& off : k.offs){
					off.off.swapXY();
				}
				return k;
			}()
		};

		export namespace Factory{
			Graphic::GraphicPostProcessorFactory game_uiMerge{};
			Graphic::GraphicPostProcessorFactory nfaaFactory_Legacy{};

			Graphic::ComputePostProcessorFactory gaussianFactory{};
			Graphic::ComputePostProcessorFactory ssaoFactory{};
			Graphic::ComputePostProcessorFactory worldMergeFactory{};
			Graphic::ComputePostProcessorFactory nfaaFactory{};
			Graphic::ComputePostProcessorFactory presentMerge{};
		}

		export void load(const Core::Vulkan::Context& context);

		export void dispose();
	}
}

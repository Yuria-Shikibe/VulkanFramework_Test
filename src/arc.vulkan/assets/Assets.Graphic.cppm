module;

#include <vulkan/vulkan.h>

export module Assets.Graphic;

import Core.Vulkan.Shader;
import Core.Vulkan.Sampler;

import Core.File;

namespace Core::Vulkan{
	export class Context;
}


export namespace Assets{
	namespace Shader{
		namespace Vert{
			Core::Vulkan::ShaderModule batchShader{};
			Core::Vulkan::ShaderModule blitSingle{};
			Core::Vulkan::ShaderModule blitWithUV{};

		    Core::Vulkan::ShaderModule uiBatch{};

		    Core::Vulkan::ShaderModule worldBatch{};
		}

		namespace Frag{
			Core::Vulkan::ShaderModule blitSingle{};
			Core::Vulkan::ShaderModule batchShader{};

			Core::Vulkan::ShaderModule blitBlur{};

			Core::Vulkan::ShaderModule blitMerge{};

			Core::Vulkan::ShaderModule uiMerge{};
			Core::Vulkan::ShaderModule uiBatch{};

			Core::Vulkan::ShaderModule worldBatch{};


			Core::Vulkan::ShaderModule game_ui_merge{};

			Core::Vulkan::ShaderModule FXAA{};
			Core::Vulkan::ShaderModule NFAA{};
			Core::Vulkan::ShaderModule SSAO{};
		}

		namespace Comp{
			Core::Vulkan::ShaderModule Gaussian{};
		}

		void load(VkDevice device);

		void dispose(){
			Vert::batchShader = {};
			Vert::blitSingle = {};
			Vert::blitWithUV = {};
			Vert::uiBatch = {};

			Frag::batchShader = {};
			Frag::blitBlur = {};
			Frag::blitMerge = {};
			Frag::blitSingle = {};

			Frag::uiMerge = {};
			Frag::uiBatch = {};

			Frag::game_ui_merge = {};

			Frag::FXAA = {};
			Frag::NFAA = {};
			Frag::SSAO = {};

			Vert::worldBatch = {};
			Frag::worldBatch = {};


			Comp::Gaussian = {};
		}
	}

	namespace Sampler{
		Core::Vulkan::Sampler textureDefaultSampler{};
		Core::Vulkan::Sampler textureNearestSampler{};
		Core::Vulkan::Sampler textureLowpSampler{};
		Core::Vulkan::Sampler blitSampler{};
		Core::Vulkan::Sampler depthSampler{};

		void load(VkDevice device);

		void dispose(){
			textureDefaultSampler = {};
			textureNearestSampler = {};
			textureLowpSampler = {};
			blitSampler = {};
			depthSampler = {};
		}
	}

	void load(const Core::Vulkan::Context& context);

	void dispose();
}

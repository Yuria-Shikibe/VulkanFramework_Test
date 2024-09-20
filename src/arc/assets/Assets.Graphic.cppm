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
			Core::Vulkan::ShaderModule blitSingle{};
			Core::Vulkan::ShaderModule blitWithUV{};

		    Core::Vulkan::ShaderModule uiBatch{};

		    Core::Vulkan::ShaderModule worldBatch{};
		}

		namespace Frag{
			Core::Vulkan::ShaderModule blitSingle{};

			Core::Vulkan::ShaderModule uiBatch{};

			Core::Vulkan::ShaderModule worldBatch{};
		}

		namespace Comp{
			Core::Vulkan::ShaderModule Gaussian{};
			Core::Vulkan::ShaderModule worldMerge{};
			Core::Vulkan::ShaderModule SSAO{};
			Core::Vulkan::ShaderModule NFAA{};
			Core::Vulkan::ShaderModule presentMerge{};
			Core::Vulkan::ShaderModule uiMerge{};
		}

		void load(VkDevice device);

		void dispose(){
			Vert::blitSingle = {};
			Vert::blitWithUV = {};
			Vert::uiBatch = {};

			Frag::blitSingle = {};

			Frag::uiBatch = {};

			Vert::worldBatch = {};
			Frag::worldBatch = {};

			Comp::worldMerge = {};
			Comp::Gaussian = {};
			Comp::SSAO = {};
			Comp::NFAA = {};
			Comp::uiMerge = {};
			Comp::presentMerge = {};
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

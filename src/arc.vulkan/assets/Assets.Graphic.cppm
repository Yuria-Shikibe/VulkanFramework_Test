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
		}

		namespace Frag{
			Core::Vulkan::ShaderModule blitSingle{};
			Core::Vulkan::ShaderModule batchShader{};

			Core::Vulkan::ShaderModule blitBlur{};

			Core::Vulkan::ShaderModule blitMerge{};

			Core::Vulkan::ShaderModule FXAA{};
			Core::Vulkan::ShaderModule NFAA{};
			Core::Vulkan::ShaderModule SSAO{};
		}

		void load(VkDevice device);

		void dispose(){
			Vert::batchShader = {};
			Vert::blitSingle = {};
			Vert::blitWithUV = {};

			Frag::batchShader = {};
			Frag::blitBlur = {};
			Frag::blitMerge = {};
			Frag::blitSingle = {};
			Frag::FXAA = {};
			Frag::NFAA = {};
			Frag::SSAO = {};
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

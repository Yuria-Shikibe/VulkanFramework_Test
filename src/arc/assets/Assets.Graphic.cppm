module;

#include <vulkan/vulkan.h>

export module Assets.Graphic;

import Core.Vulkan.Shader;
import Core.Vulkan.Sampler;

import OS.File;

namespace Core::Vulkan{
	export class Context;
}


export namespace Assets{
	namespace Shader{
		OS::File builtinShaderDir{};
		Core::Vulkan::ShaderModule batchVertShader{};
		Core::Vulkan::ShaderModule batchFragShader{};

		Core::Vulkan::ShaderModule blitSingleFrag{};
		Core::Vulkan::ShaderModule blitSingleVert{};

		Core::Vulkan::ShaderModule blitBlurFrag{};
		Core::Vulkan::ShaderModule blitBlurVert{};

		Core::Vulkan::ShaderModule blitMergeFrag{};

		void load(VkDevice device);

		void dispose(){
			batchVertShader = {};
			batchFragShader = {};
			blitSingleFrag = {};
			blitSingleVert = {};

			blitBlurFrag = {};
			blitBlurVert = {};
			blitMergeFrag = {};
		}
	}

	namespace Sampler{
		Core::Vulkan::Sampler textureDefaultSampler{};
		Core::Vulkan::Sampler blitSampler{};

		void load(VkDevice device);

		void dispose(){
			textureDefaultSampler = {};
			blitSampler = {};
		}
	}

	void load(const Core::Vulkan::Context& context);

	void dispose();
}

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

		namespace Vert{
			Core::Vulkan::ShaderModule batchShader{};
			Core::Vulkan::ShaderModule blitSingle{};
			Core::Vulkan::ShaderModule blitUV{};
		}

		namespace Frag{
			Core::Vulkan::ShaderModule blitSingle{};
			Core::Vulkan::ShaderModule batchShader{};

			Core::Vulkan::ShaderModule blitBlur{};

			Core::Vulkan::ShaderModule blitMerge{};

			Core::Vulkan::ShaderModule FXAA{};
			Core::Vulkan::ShaderModule NFAA{};
		}




		void load(VkDevice device);

		void dispose(){
			Vert::batchShader = {};
			Vert::blitSingle = {};
			Vert::blitUV = {};

			Frag::batchShader = {};
			Frag::blitBlur = {};
			Frag::blitMerge = {};
			Frag::blitSingle = {};
			Frag::FXAA = {};
			Frag::NFAA = {};
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

module;

#include <vulkan/vulkan.h>

export module Assets.Graphic;

import Core.Vulkan.Shader;
import Core.Vulkan.Sampler;

import OS.File;

export namespace Assets{
	namespace Shader{
		OS::File builtinShaderDir{};
		Core::Vulkan::ShaderModule batchVertShader{};
		Core::Vulkan::ShaderModule batchFragShader{};
		Core::Vulkan::ShaderModule blitSingleFrag{};
		Core::Vulkan::ShaderModule blitSingleVert{};

		void load(VkDevice device);

		void dispose(){
			batchVertShader = {};
			batchFragShader = {};
			blitSingleFrag = {};
			blitSingleVert = {};
		}
	}

	namespace Sampler{
		Core::Vulkan::Sampler textureDefaultSampler{};

		void load(VkDevice device);

		void dispose(){
			textureDefaultSampler = {};
		}
	}

	void load(VkDevice device){
		Shader::load(device);
		Sampler::load(device);
	}

	void dispose(){
		Shader::dispose();
		Sampler::dispose();
	}
}

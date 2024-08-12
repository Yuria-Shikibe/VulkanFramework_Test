module;

#include <vulkan/vulkan.h>

export module Assets.Graphic;

import Core.Vulkan.Shader;

import OS.File;

export namespace Assets{
	namespace Shader{
		OS::File builtinShaderDir{};
		Core::Vulkan::ShaderModule batchVertShader{};
		Core::Vulkan::ShaderModule batchFragShader{};

		void load(VkDevice device);

		void dispose(){
			batchVertShader = {};
			batchFragShader = {};
		}
	}
}

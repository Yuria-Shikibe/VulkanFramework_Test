module;

#include <vulkan/vulkan.h>

module Assets.Graphic;

import Core.Vulkan.Context;
import Assets.Graphic.PostProcess;

void Assets::Shader::load(VkDevice device){
	batchVertShader = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(test.vert.spv)"
	};

	batchFragShader = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(test.frag.spv)"
	};

	blitSingleVert = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(blit.vert.spv)"
	};

	blitSingleFrag = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(blit.frag.spv)"
	};

	blitBlurFrag = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(blit_blur.frag.spv)"
	};

	blitBlurVert = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(blit_blur.vert.spv)"
	};

	blitMergeFrag = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(blit_merge.frag.spv)"
	};
}

void Assets::Sampler::load(VkDevice device){
	textureDefaultSampler = Core::Vulkan::Sampler(device, Core::Vulkan::SamplerInfo::TextureSampler);
	blitSampler = Core::Vulkan::Sampler(device, VkSamplerCreateInfo{}
		| Core::Vulkan::SamplerInfo::Default
		| Core::Vulkan::SamplerInfo::LOD_None
		| Core::Vulkan::SamplerInfo::AddressMode_Clamp
		| Core::Vulkan::SamplerInfo::Filter_Linear
		| Core::Vulkan::SamplerInfo::Anisotropy<0>
		| Core::Vulkan::SamplerInfo::CompareOp<>
	);
}

void Assets::load(const Core::Vulkan::Context& context){
	Shader::load(context.device);
	Sampler::load(context.device);
	PostProcess::load(context);
}

void Assets::dispose(){
	Shader::dispose();
	Sampler::dispose();
}
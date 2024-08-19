module;

#include <vulkan/vulkan.h>

module Assets.Graphic;

import Core.Vulkan.Context;
import Assets.Graphic.PostProcess;

void Assets::Shader::load(VkDevice device){
	Vert::batchShader = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(test.vert.spv)"
	};

	Frag::batchShader = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(test.frag.spv)"
	};

	Vert::blitSingle = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(blit.vert.spv)"
	};

	Frag::blitSingle = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(blit.frag.spv)"
	};

	Frag::blitBlur = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(blit_blur.frag.spv)"
	};

	Vert::blitUV = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(blit_uv.vert.spv)"
	};

	Frag::blitMerge = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(blit_merge.frag.spv)"
	};

	Frag::FXAA = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(fxaa.frag.spv)"
	};

	Frag::NFAA = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(nfaa.frag.spv)"
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
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

	Vert::blitWithUV = Core::Vulkan::ShaderModule{
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

	Frag::SSAO = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(ssao.frag.spv)"
	};
}

void Assets::Sampler::load(VkDevice device){
	textureDefaultSampler = Core::Vulkan::Sampler(device, Core::Vulkan::SamplerInfo::TextureSampler);

    textureLowpSampler = Core::Vulkan::Sampler(device, VkSamplerCreateInfo{}
		| Core::Vulkan::SamplerInfo::Default
		| Core::Vulkan::SamplerInfo::LOD_Max
		| Core::Vulkan::SamplerInfo::AddressMode_Clamp
		| Core::Vulkan::SamplerInfo::Filter_Nearest
		| Core::Vulkan::SamplerInfo::Anisotropy<0>
		| Core::Vulkan::SamplerInfo::CompareOp<>
	);

    blitSampler = Core::Vulkan::Sampler(device, VkSamplerCreateInfo{}
		| Core::Vulkan::SamplerInfo::Default
		| Core::Vulkan::SamplerInfo::LOD_None
		| Core::Vulkan::SamplerInfo::AddressMode_Clamp
		| Core::Vulkan::SamplerInfo::Filter_Linear
		| Core::Vulkan::SamplerInfo::Anisotropy<0>
		| Core::Vulkan::SamplerInfo::CompareOp<>
	);

    // auto info = VkSamplerCreateInfo{}
    // | Core::Vulkan::SamplerInfo::Default
    // | Core::Vulkan::SamplerInfo::LOD_None
    // | Core::Vulkan::SamplerInfo::AddressMode_Clamp
    // | Core::Vulkan::SamplerInfo::Filter_Linear
    // | Core::Vulkan::SamplerInfo::Anisotropy<0>
    // | Core::Vulkan::SamplerInfo::CompareOp<>;
    //
    // info.
    //
    // depthSampler = Core::Vulkan::Sampler(device, info);
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
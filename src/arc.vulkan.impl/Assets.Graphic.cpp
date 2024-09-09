module;

#include <vulkan/vulkan.h>

module Assets.Graphic;
import Assets.Directories;

import Core.Vulkan.Context;
import Assets.Graphic.PostProcess;

import std;

void Assets::Shader::load(const VkDevice device){
    const auto& dir = Dir::shader_spv;

	Vert::blitSingle = Core::Vulkan::ShaderModule{
		device, dir / R"(blit.vert.spv)"
	};

	Vert::uiBatch = Core::Vulkan::ShaderModule{
		device, dir / R"(ui.batch.vert.spv)"
	};

	Frag::blitSingle = Core::Vulkan::ShaderModule{
		device, dir / R"(blit.frag.spv)"
	};

	Vert::blitWithUV = Core::Vulkan::ShaderModule{
		device, dir / R"(blit_uv.vert.spv)"
	};

    Frag::uiBatch = Core::Vulkan::ShaderModule{
        device, dir / R"(ui.batch.frag.spv)"
    };


    Vert::worldBatch = Core::Vulkan::ShaderModule{
        device, dir / R"(world.vert.spv)"
    };

    Frag::worldBatch = Core::Vulkan::ShaderModule{
        device, dir / R"(world.frag.spv)"
    };


    Comp::Gaussian = Core::Vulkan::ShaderModule{
        device, dir / R"(gaussian.comp.spv)"
    };

    Comp::SSAO = Core::Vulkan::ShaderModule{
        device, dir / R"(ssao.comp.spv)"
    };

    Comp::NFAA = Core::Vulkan::ShaderModule{
        device, dir / R"(nfaa.comp.spv)"
    };

    Comp::worldMerge = Core::Vulkan::ShaderModule{
        device, dir / R"(world.merge.comp.spv)"
    };

    Comp::presentMerge = Core::Vulkan::ShaderModule{
        device, dir / R"(present.merge.comp.spv)"
    };

    Comp::uiMerge = Core::Vulkan::ShaderModule{
        device, dir / R"(ui.merge.comp.spv)"
    };
}

void Assets::Sampler::load(const VkDevice device){
    textureDefaultSampler = Core::Vulkan::Sampler(device, Core::Vulkan::SamplerInfo::TextureSampler);

    textureNearestSampler = Core::Vulkan::Sampler(device, VkSamplerCreateInfo{}
        | Core::Vulkan::SamplerInfo::Default
        | Core::Vulkan::SamplerInfo::Filter_PIXEL_Linear
        | Core::Vulkan::SamplerInfo::AddressMode_Clamp
        | Core::Vulkan::SamplerInfo::LOD_Max_Nearest
        | Core::Vulkan::SamplerInfo::CompareOp<VK_COMPARE_OP_NEVER>
        | Core::Vulkan::SamplerInfo::Anisotropy<0>);

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
	std::println("[Assets] Graphic: Begin Assets Loading");
	Shader::load(context.device);
	std::println("[Assets] Graphic: Shader Complete");

	Sampler::load(context.device);
	PostProcess::load(context);
	std::println("[Assets] Graphic: Assets Loaded");
}

void Assets::dispose(){
	Shader::dispose();
	Sampler::dispose();
}
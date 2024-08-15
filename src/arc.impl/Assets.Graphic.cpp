module Assets.Graphic;

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
}

void Assets::Sampler::load(VkDevice device){
	textureDefaultSampler = Core::Vulkan::Sampler(device, Core::Vulkan::SamplerInfo::TextureSampler);
}

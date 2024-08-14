module Assets.Graphic;

void Assets::Shader::load(VkDevice device){
	batchVertShader = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(test.vert.spv)"
	};

	batchFragShader = Core::Vulkan::ShaderModule{
		device, builtinShaderDir / R"(test.frag.spv)"
	};
}

void Assets::Sampler::load(VkDevice device){
	textureDefaultSampler = Core::Vulkan::Sampler(device, Core::Vulkan::SamplerInfo::TextureSampler);
}

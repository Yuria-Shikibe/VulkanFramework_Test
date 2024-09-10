
export module MainTest;

import Core.Vulkan.Shader.Compile;
import Core.File;

import Assets.Directories;

import Graphic.Batch;

import std;

export namespace Test{
	void compileAllShaders() {
		Core::Vulkan::ShaderRuntimeCompiler compiler{};
		compiler.addMarco("MaximumAllowedSamplersSize", std::format("{}", Graphic::Batch::MaximumAllowedSamplersSize));
		const Core::Vulkan::ShaderCompilerWriter adaptor{compiler, Assets::Dir::shader_spv};

		Core::File{Assets::Dir::shader_src}.forSubs([&](Core::File&& file){
			if(file.extension().empty())return;
			adaptor.compile(file);
		});
	}
}
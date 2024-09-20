
export module MainTest;

import Core.Vulkan.Shader.Compile;
import Core.File;

import Assets.Directories;

import Graphic.Batch;



import ext.meta_programming;

import std;



import Core.UI.Root;
import Core.UI.BedFace;
import Core.UI.ScrollPanel;
import Core.UI.CellEmbryo;

import Core.Global.UI;

export namespace Test{
	void initDefUI(){
		auto& bed = Core::Global::UI::getMainGroup();

		// bed.emplaceInit<Core::UI::ScrollPanel>([](Core::UI::ScrollPanel& v){
		//
		// }).setScale({{}, {0.3f, 0.5f}}).setMargin(10.).setAlign(Align::Pos::center_left);
		//
		// bed.emplaceInit<Core::UI::Element>([](Core::UI::Element& v){

		// }).setScale({{0.f, 0.4f}, {0.5f, 0.5f}}).setMargin(10.).setAlign(Align::Pos::top_center);

		bed.emplaceInit<Core::UI::Embryo_Saturated>([](Core::UI::Embryo_Saturated& v){
			v.emplaceInit<Core::UI::ScrollPanel>([](Core::UI::ScrollPanel& panel){
				panel.setItem<Core::UI::Element>([](Core::UI::Element& e){
					e.resize({800, 1500});
				});
			}).setMargin(10);
			v.endRow().emplace<Core::UI::Element>().setMargin(10);
			v.endRow().emplace<Core::UI::Element>().setMargin(10);
			v.endRow().emplace<Core::UI::Element>().setMargin(10);
			v.emplace<Core::UI::Element>().setMargin(10);
			v.endRow().emplace<Core::UI::Element>().setMargin(10);

		}).setScale({Geom::FromExtent, {0.f, 0.0f}, {0.2f, 0.8f}}).setMargin(10).setAlign(Align::Pos::top_right);
	}

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

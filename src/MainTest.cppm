export module MainTest;

import Core.Vulkan.Shader.Compile;
import Core.File;

import Assets.Directories;

import Graphic.Batch.MultiThread;



import ext.meta_programming;

import std;



import Core.UI.Root;
import Core.UI.BedFace;
import Core.UI.ScrollPanel;
import Core.UI.CellEmbryo;
import Core.UI.Table;

import Core.Global.UI;

export namespace Test{
	void initDefUI(){
		auto& bed = Core::Global::UI::getMainGroup();

		bed.emplaceInit<Core::UI::Embryo_Unsaturated>([](Core::UI::Embryo_Unsaturated& embryo){
			embryo.emplaceInit<Core::UI::FixedTable>([](Core::UI::FixedTable& v){
				v.align = Align::Pos::top_left;

				v.emplace<Core::UI::Element>().setSize(80.).setMargin(5);
				v.emplace<Core::UI::Element>().setSize(60.).setMargin(5);
				v.emplace<Core::UI::Element>().setMargin(5);
				v.endRow();
				v.emplace<Core::UI::Element>().setSize(30., 120.).setMargin(5);
				v.emplace<Core::UI::Element>().setWidth(90).setMargin(5);
				v.endRow();
				v.emplace<Core::UI::Element>().setSize(100., 80.).setMargin(5);
				v.emplace<Core::UI::Element>().setMargin(5);
				v.emplace<Core::UI::Element>().setMargin(5);
				v.endRow();
				v.emplace<Core::UI::Element>().setMargin(5);
				v.emplace<Core::UI::Element>().setMargin(5);
				v.emplace<Core::UI::Element>().setWidth(120).setRatio_y(1.f).setMargin(5);
				v.emplace<Core::UI::Element>().setMargin(5);
			});
			embryo.endRow();
			embryo.emplaceInit<Core::UI::ScrollPanel>([](Core::UI::ScrollPanel& pane){
				pane.setItem<Core::UI::FlexTable>([](Core::UI::FlexTable& v){
					v.align = Align::Pos::top_left;

					v.emplace<Core::UI::Element>().setSize(80.).setMargin(5);
					v.emplace<Core::UI::Element>().setSize(60.).setMargin(5);
					v.emplace<Core::UI::Element>().setMargin(5);
					v.endRow();
					v.emplace<Core::UI::Element>().setSize(30., 120.).setMargin(5);
					v.emplace<Core::UI::Element>().setWidth(90).setMargin(5);
					v.endRow();
					v.emplace<Core::UI::Element>().setSize(100., 80.).setMargin(5);
					v.emplace<Core::UI::Element>().setMargin(5);
					v.emplace<Core::UI::Element>().setMargin(5);
					v.endRow();
					v.emplace<Core::UI::Element>().setMargin(5);
					v.emplace<Core::UI::Element>().setMargin(5);
					v.emplace<Core::UI::Element>().setWidth(120).setRatio_y(1.f).setMargin(5);
					v.emplace<Core::UI::Element>().setMargin(5);
				});
			});
		}).setScale({Geom::FromExtent, {0.f, 0.0f}, {0.2f, 1.f}}).setMargin(10).setAlign(Align::Pos::top_left);


		//
		// bed.emplaceInit<Core::UI::Element>([](Core::UI::Element& v){

		// }).setScale({{0.f, 0.4f}, {0.5f, 0.5f}}).setMargin(10.).setAlign(Align::Pos::top_center);

		// bed.emplaceInit<Core::UI::Embryo_Saturated>([](Core::UI::Embryo_Saturated& v){
		// 	v.emplaceInit<Core::UI::ScrollPanel>([](Core::UI::ScrollPanel& panel){
		// 		panel.setItem<Core::UI::Element>([](Core::UI::Element& e){
		// 			e.resize({800, 1500});
		// 		});
		// 	}).setMargin(10);
		// 	v.endRow().emplace<Core::UI::Element>().setMargin(10);
		// 	v.endRow().emplace<Core::UI::Element>().setMargin(10);
		// 	v.endRow().emplace<Core::UI::Element>().setMargin(10);
		// 	v.emplace<Core::UI::Element>().setMargin(10);
		// 	v.endRow().emplace<Core::UI::Element>().setMargin(10);
		//
		// }).setScale({Geom::FromExtent, {0.f, 0.0f}, {0.2f, 0.8f}}).setMargin(10).setAlign(Align::Pos::top_right);
	}

	void compileAllShaders(){
		Core::Vulkan::ShaderRuntimeCompiler compiler{};
		compiler.addMarco("MaximumAllowedSamplersSize",
		                  std::format("{}", Graphic::Batch_MultiThread::MaximumAllowedSamplersSize));
		const Core::Vulkan::ShaderCompilerWriter adaptor{compiler, Assets::Dir::shader_spv};

		Core::File{Assets::Dir::shader_src}.forSubs([&](Core::File&& file){
			if(file.extension().empty()) return;
			adaptor.compile(file);
		});
	}
}

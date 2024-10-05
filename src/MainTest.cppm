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
import Core.UI.TextElement;
import Core.UI.Slider;

import Core.Global.UI;

export namespace Test{
	static_assert(Core::UI::ElemInitFunc<decltype([](int, Core::UI::BedFace&){})>);

	void initDefUI(){
		//return;

		auto& group = Core::Global::UI::getMainGroup();

		auto& Bed = group.emplaceInitChildren([](Core::UI::BedFace& v){
			v.prop().fillParent = {true, true};
		});

		// bed.emplaceInit<Core::UI::TextElement>([](Core::UI::TextElement& v){
		// 	v.textAlignMode = Align::Pos::center;
		// 	v.setText("叮咚鸡叮咚鸡大狗大狗叫叫叫");
		// }).setScale({Geom::FromExtent, {0.f, 0.0f}, {0.2f, 0.6f}}).setMargin(10).setAlign(Align::Pos::center);;
		Bed.emplaceInit([](Core::UI::Embryo_Unsaturated& embryo){
			// embryo.emplaceInit<Core::UI::TextElement>([](Core::UI::TextElement& v){
			// 	v.glyphSizeDependency = {false, false};
			// 	v.resize({500, 500});
			// 	v.setText("ajsdhashdjahjsdhasdh;hjgfhajshgj");
			// });
			// embryo.endRow();
			embryo.emplaceInit([](Core::UI::ScrollPanel& pane){
				pane.setItem([](Core::UI::FlexTable_Dynamic& v){
					v.align = Align::Pos::top_left;
					v.emplaceInit([](Core::UI::Slider& slider){
						slider.setCallback([](Core::UI::Slider& s){
							std::println(std::cout, "{}", s.getProgress());
							std::cout.flush();
						});
					}).setWidth(500).setMargin(5);

					v.emplaceInit([](Core::UI::TextElement& e){
						e.glyphSizeDependency = {true, true};
						// e.resize({500, 500});
						e.setText("ajsdhash\n叮咚鸡大狗叫djahjsdhasdh;hjgfhajshgj123123123");
						e.textAlignMode = Align::Pos::top_left;
					}).setExternal({true, true}).setMargin(5);
				});
			});
		}).setScale({Geom::FromExtent, {0.f, 0.0f}, {0.56f, 1.f}}).setMargin(10).setAlign(Align::Pos::top_left);


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

export module MainTest;

import Core.Vulkan.Shader.Compile;
import Core.File;

import Assets.Directories;

import Graphic.Batch.MultiThread;

import ext.meta_programming;

import std;

import Core.Global;

export import Core.UI.Root;
export import Core.UI.BedFace;
export import Core.UI.ScrollPanel;
export import Core.UI.CellEmbryo;
export import Core.UI.Table;
export import Core.UI.TextElement;
export import Core.UI.ImageDisplay;
export import Core.UI.Slider;
export import Core.UI.RegionDrawable;

export import Core.Global.UI;
export import Core.Global;
export import Core.Global.Graphic;
export import Core.Global.Assets;
import Core.InitAndTerminate;

export import Graphic.Renderer.UI;
export import Graphic.Renderer.World;
export import Graphic.Color;
export import Graphic.ImageRegion;
export import Graphic.ImageNineRegion;
export import Graphic.Batch.Exclusive;
export import Graphic.Draw.Func;
export import Graphic.Draw.NinePatch;
export import Graphic.Batch.AutoDrawParam;
export import Graphic.Pixmap;
export import Graphic.ImageAtlas;

import Core.Vulkan.Texture;

export namespace Test{
	static_assert(Core::UI::ElemInitFunc<decltype([](int, Core::UI::BedFace&){})>);

	Core::Vulkan::Texture texturePester{};
	Core::Vulkan::Texture texturePesterLight{};

	Graphic::ImageViewNineRegion nineRegion_edge{};
	Graphic::ImageViewNineRegion nineRegion_base{};

	void loadTex(){
		using namespace Core::Global::Asset;
		atlas->registerNamedImageViewRegionGuaranteed("ui.base", Graphic::Pixmap{Assets::Dir::texture.find("ui/elem-s1-back.png")});
		atlas->registerNamedImageViewRegionGuaranteed("ui.edge", Graphic::Pixmap{Assets::Dir::texture.find("ui/elem-s1-edge.png")});
		atlas->registerNamedImageViewRegionGuaranteed("ui.cent", Graphic::Pixmap{Assets::Dir::texture.find("test-1.png")});

		nineRegion_base = {atlas->at("ui.base"), Align::Pad<std::uint32_t>{8, 8, 8, 8}};
		nineRegion_edge = {
			atlas->at("ui.edge"),
			Align::Pad<std::uint32_t>{8, 8, 8, 8},
			atlas->at("ui.cent"),
			// {200, 200},
			Align::Scale::clamped
		};
	}

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

						slider.setTooltipState({
							.followTarget = Core::UI::TooltipFollow::depends,
							.followTargetAlign = Align::Pos::bottom_left,
							.tooltipSrcAlign = Align::Pos::top_left,
						}, [](Core::UI::FlexTable_Dynamic& table){
							table.defaultCell.setSize(220.f, 80.f).setMargin(5.f);
							table.emplaceInit([](Core::UI::Slider& slider){
								slider.setHoriOnly();
							});

							table.emplaceInit([](Core::UI::Slider& slider){
								slider.setHoriOnly();
							});

							table.endRow();

							table.emplaceInit([](Core::UI::TextElement& text){
								text.setText("叮咚鸡ahsf;hl");
								text.setTextScale(.8f);
							});

							table.emplaceInit([](Core::UI::ImageDisplay& display){
								display.setDrawable(Core::UI::TextureNineRegionDrawable_Ref{&nineRegion_edge});
								display.scaling = Align::Scale::stretch;
							});

							table.emplace<Core::UI::ImageDisplay>(
								Core::UI::TextureNineRegionDrawable_Ref{&nineRegion_edge},
								Align::Scale::stretch);
						});
					}).setWidth(500).setMargin(5);

					v.emplaceInit([](Core::UI::TextElement& e){
						e.glyphSizeDependency = {true, true};
						// e.resize({500, 500});
						e.setTextScale(.5);
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

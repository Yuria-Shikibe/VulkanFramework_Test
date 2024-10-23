export module MainTest;

import Core.Vulkan.Shader.Compile;
import Core.File;

import Assets.Directories;

import Graphic.Batch.MultiThread;

import ext.meta_programming;

import std;

export import Core.UI.Root;
export import Core.UI.BedFace;
export import Core.UI.ScrollPanel;
export import Core.UI.CellEmbryo;
export import Core.UI.Table;
export import Core.UI.TextElement;
export import Core.UI.ImageDisplay;
export import Core.UI.Slider;
export import Core.UI.RegionDrawable;
export import Core.UI.Button;

export import Core.UI.Styles;

import Core.Input;
import Core.Window;
import Graphic.Camera2D;

export import Core.Global.UI;
export import Core.Global;
export import Core.Global.Graphic;
export import Core.Global.Assets;
import Core.InitAndTerminate;
import Core.Ctrl;

export import Graphic.Renderer.UI;
export import Graphic.Renderer.World;
export import Graphic.Color;
export import Graphic.ImageRegion;
export import Graphic.ImageNineRegion;
export import Graphic.Batch.Exclusive;
export import Graphic.Draw.Func;
export import Graphic.Draw.NinePatch;
export import Graphic.Batch.AutoDrawParam;
export import Graphic.ImageAtlas;
export import Graphic.Pixmap;

import Core.Vulkan.Texture;

import Game.Delay;

export import Assets.Fx;
export import Game.World.State;
export import Game.World.RealEntity;

import ext.timer;
import ext.shared_stack;

export namespace Test{
	static_assert(Core::UI::ElemInitFunc<decltype([](int, Core::UI::BedFace&){})>);

	Graphic::ImageViewRegion* texturePester{};
	Graphic::ImageViewRegion* texturePesterLight{};

	Graphic::ImageViewNineRegion nineRegion_edge{};
	Graphic::ImageViewNineRegion nineRegion_base{};

	constexpr Core::UI::Style::Palette PaletteFront{
		.general = Graphic::Colors::AQUA,
		.onFocus = Graphic::Colors::AQUA.copy().lerp(Graphic::Colors::WHITE, 0.4f),
		.onPress = Graphic::Colors::WHITE,
		.disabled = {},
		.activated = {}
	};

	constexpr Core::UI::Style::Palette PaletteBack{
		.general = Graphic::Colors::BLACK,
		.onFocus = Graphic::Colors::DARK_GRAY,
		.onPress = Graphic::Colors::GRAY,
		.disabled = {},
		.activated = {}
	};

	Core::UI::Style::RoundStyle defaultStyle{};

	[[nodiscard]] Geom::Vec2 getMouseToWorld(){
		return Core::Global::mainCamera->getScreenToWorld() * (Core::Global::input->getCursorPos() / Core::Global::window->getSize().as<float>()).sub(.5f, .5f).mul(2, -2);
	}

	Game::DelayActionManager delayManager{};

	void update(float delta){
		delayManager.update(delta);
	}

	void initFx(){
		using namespace Core;
		Global::input->binds.registerBind(Ctrl::InputBind{
				Ctrl::Mouse::_1, Ctrl::Act::Press, []{
					delayManager.launch(Game::ActionPriority::unignorable, {
						.delay = 45.f,
						.repeat = 5,
						.noSuspend = true
					}, [pos = getMouseToWorld()]{
						Assets::Fx::CircleOut.launch({
							.zLayer = 0.2f,
							.trans = {pos},
							.color = Graphic::Colors::AQUA_SKY,
						});
					});
				}
			});
	}

	void loadTex(){
		using namespace Core::Global::Asset;
		atlas->registerNamedImageViewRegionGuaranteed("ui.base", Graphic::Pixmap{Assets::Dir::texture.find("ui/elem-s1-back.png")});
		atlas->registerNamedImageViewRegionGuaranteed("ui.edge", Graphic::Pixmap{Assets::Dir::texture.find("ui/elem-s1-edge.png")});
		atlas->registerNamedImageViewRegionGuaranteed("ui.cent", Graphic::Pixmap{Assets::Dir::texture.find("test-1.png")});

		texturePester = &atlas->registerNamedImageViewRegionGuaranteed("main.pester", Graphic::Pixmap{Assets::Dir::texture / R"(pester.png)"}).first;
		texturePesterLight = &atlas->registerNamedImageViewRegionGuaranteed("main.pester.light", Graphic::Pixmap{Assets::Dir::texture / R"(pester.light.png)"}).first;

		nineRegion_base = {atlas->at("ui.base"), Align::Pad<std::uint32_t>{8, 8, 8, 8}};
		nineRegion_edge = {
			atlas->at("ui.edge"),
			Align::Pad<std::uint32_t>{8, 8, 8, 8},
			// atlas->at("ui.cent"),
			{0, 0},
			Align::Scale::none
		};

		defaultStyle.edge = Core::UI::Style::PaletteWith{nineRegion_edge, PaletteFront};
		defaultStyle.base = Core::UI::Style::PaletteWith{nineRegion_base, Core::UI::Style::Palette{PaletteBack}.mulAlpha(0.6f)};

		Core::UI::GlobalStyleDrawer = &defaultStyle;

	}

	void initDefUI(){
		//return;

		auto& group = Core::Global::UI::getMainGroup();

		auto& Bed = group.emplaceInitChildren([](Core::UI::BedFace& v){
			v.prop().fillParent = {true, true};
			v.prop().graphicData.setEmptyDrawer();
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
					v.defaultCell.setMargin({.right = 5.f});
					v.emplaceInit([](Core::UI::Slider& slider){
						slider.prop().boarder.set(5);
						slider.setCallback([](Core::UI::Slider& s){
							std::println(std::cout, "{}", s.getProgress());
							std::cout.flush();
						});

						slider.setTooltipState({
							.followTarget = Core::UI::TooltipFollow::depends,
							.followTargetAlign = Align::Pos::bottom_left,
							.tooltipSrcAlign = Align::Pos::top_left,
						}, [](Core::UI::FlexTable_Dynamic& table){
							table.defaultCell.setSize(220.f, 80.f).setMargin(2.f);

							table.emplaceInit([](Core::UI::Slider& slider){
								slider.setHoriOnly();
							});

							table.emplaceInit([](Core::UI::Slider& slider){
								slider.setHoriOnly();
							});

							table.emplaceInit([](Core::UI::Button<Core::UI::ImageDisplay>& display){
								display.setDrawable(Core::UI::TextureNineRegionDrawable_Ref{&nineRegion_edge});
								display.scaling = Align::Scale::stretch;
								display.setButtonCallback(Core::UI::ButtonTag::Normal, []{
									std::println(std::cout, "pressed");
									std::cout.flush();
								});
							});

							table.endRow();

							table.emplaceInit([](Core::UI::TextElement& text){
								text.setText("叮咚鸡ahsf;hl");
								text.setTextScale(.8f);
							});

							table.emplaceInit([](Core::UI::FixedImageDisplay<Core::UI::TextureNineRegionDrawable_Ref>& display){
								display.setDrawable(Core::UI::TextureNineRegionDrawable_Ref{&nineRegion_edge});
								display.scaling = Align::Scale::stretch;
							});

							table.emplace<Core::UI::FixedImageDisplay<Core::UI::TextureNineRegionDrawable_Ref>>(
								Core::UI::TextureNineRegionDrawable_Ref{&nineRegion_edge},
								Align::Scale::stretch);
						});
					}).setWidth(500);

					v.emplaceInit([](Core::UI::TextElement& e){
						e.glyphSizeDependency = {true, true};
						// e.resize({500, 500});
						e.setTextScale(.5);
						e.prop().boarder.set(5);
						e.setText("ajsdhash\n叮咚鸡大狗叫djahjsdhasdh;hjgfhajshgj123123123");
						e.textAlignMode = Align::Pos::top_left;
					}).setExternal({true, true});
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

export namespace Test::GamePart{
	Game::WorldState world{};
	ext::timer<> timer{};
	ext::shared_stack<Game::RealEntity*> pre_collide_passed{};
	ext::shared_stack<Game::RealEntity*> post_collide_passed{};

	std::size_t testCount{};
	std::chrono::microseconds quadTreeTestTime{};
	std::chrono::microseconds manifoldProcessTime{};

	void init(){
		constexpr float size = 400000.f;
		world.quadTree = Geom::quad_tree<Game::RealEntity>{{
			{-size, -size}, {size, size}
		}};
		world.all.dumpAll();
		world.realEntities.dumpAll();
		world.updateQuadTree();

		Core::Global::input->binds.registerBind(Core::Ctrl::InputBind{
				Core::Ctrl::Key::F, Core::Ctrl::Act::Continuous, []{
					timer.update_and_run(0, 10, Core::Global::timer.updateDeltaTick(), []{
						const auto cameraPos = Core::Global::mainCamera->getPosition();
						const auto cursorPos = Test::getMouseToWorld();
						const auto dst = cursorPos - cameraPos;
						const auto ang = dst.angle();

						for(int i = 0; i <= 0; ++i){
							auto& ent = world.add([&](Game::RealEntity& entity){
								entity.rigidProp.inertialMass /= 5.f;
								entity.motion.trans = {cameraPos + dst.copy().rotateRT().normalize() * i * 100, ang};
								entity.motion.vel = {Geom::dirNor(ang) * 500};

								entity.hitbox = Game::Hitbox{
										Game::HitBoxComponent{
											.box = Geom::RectBox{
												{200, 40,},
												{-100, -20,}
											}
										},
										entity.motion.trans
									};
							});
						}

					});


				}
			});

	}

	void postUpdate(float delta){
		std::atomic_size_t potentialCount{};

		auto cur0 = std::chrono::system_clock::now();

		world.realEntities.each_par([&potentialCount](Game::RealEntity& entity){
			world.quadTree.intersect_test_all(
				entity,
				[&potentialCount](Game::RealEntity& sbj, const Game::RealEntity& obj){
					if(sbj.testIntersectionWith(obj)){
						potentialCount.fetch_add(1, std::memory_order::relaxed);
					}
				},

				[](const Game::RealEntity& sbj, const Game::RealEntity& obj){
					if(&sbj == &obj) return false;
					if(!sbj.getBound().overlap_Exclusive(obj.getBound())) return false;

					return sbj.roughIntersectWith(obj);
				});
		});

		auto cur1 = std::chrono::system_clock::now();

		post_collide_passed.clear_and_reserve(potentialCount.load() + 8);
		// post_collide_passed.clear_and_reserve(world.realEntities.size() + 8);

		world.realEntities.each([](Game::RealEntity& e){
			if(e.postProcessCollisions_0()){
					post_collide_passed.push(&e);
				}else{
					e.manifold.clear();
				}
		});

		for (Game::RealEntity* e : post_collide_passed){
			e->postProcessCollisions_1();
			e->postProcessCollisions_2();
		}

		for (Game::RealEntity* e : post_collide_passed){
			e->postProcessCollisions_3(delta);
		}

		// if(post_collide_passed.size() > 500){
		// 	std::for_each(std::execution::par, post_collide_passed.begin(), post_collide_passed.end(), [](Game::RealEntity* e){
		// 		e->postProcessCollisions_1();
		// 		e->postProcessCollisions_2();
		// 	});
		//
		// 	std::for_each(std::execution::par, post_collide_passed.begin(), post_collide_passed.end(), [delta](Game::RealEntity* e){
		// 		e->postProcessCollisions_3(delta);
		// 	});
		// }else{
		//
		// }

		auto cur2 = std::chrono::system_clock::now();

		testCount++;
		quadTreeTestTime += std::chrono::duration_cast<std::chrono::microseconds>(cur1 - cur0);
		manifoldProcessTime += std::chrono::duration_cast<std::chrono::microseconds>(cur2 - cur1);

		// world.realEntities.each_par ([](Game::RealEntity& e){
		// 	e.postProcessCollisions_0();
		// 	e.postProcessCollisions_1();
		// 	e.postProcessCollisions_2();
		// });
		//
		// world.realEntities.each_par ([delta](Game::RealEntity& e){
		// 	e.postProcessCollisions_3(delta);
		// });
	}

	void printTestPerformance(){
		std::println("entities: {}", world.realEntities.size());
		std::println("tested frames: {}", testCount);
		std::println("avg quadtree time: {}", std::chrono::duration_cast<std::chrono::microseconds>(quadTreeTestTime / testCount));
		std::println("avg manifold time: {}", std::chrono::duration_cast<std::chrono::microseconds>(manifoldProcessTime / testCount));
		std::println("avg total test time: {}", std::chrono::duration_cast<std::chrono::microseconds>((quadTreeTestTime + manifoldProcessTime) / testCount));
	}

	void update(const float delta){
		world.all.dumpAll();
		world.realEntities.dumpAll();

		if(delta == .0f)return;

		world.all.update_par(delta);

		world.updateQuadTree();
	}

	void draw(){
		world.draw(Core::Global::mainCamera->getViewport());
	}
}

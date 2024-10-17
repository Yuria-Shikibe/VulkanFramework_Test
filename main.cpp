#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

// #include "src/application_head.h"

import Core.Window;
import Core.Vulkan.Manager;
import Core.Vulkan.Instance;
import Graphic.Vertex;
import Core.Vulkan.Uniform;
import Core.Vulkan.Image;
import Core.Vulkan.Texture;
import Core.Vulkan.Attachment;
import Core.Vulkan.EXT;

import Core.File;
import Graphic.Color;
import Graphic.Batch.MultiThread;
import Graphic.Pixmap;

import Core.Input;
import Graphic.Camera2D;
import Core.InitAndTerminate;

import Geom.Matrix4D;
import Geom.GridGenerator;
import Geom.Rect_Orthogonal;
import Geom.quad_tree;

import Core.Vulkan.Shader.Compile;

import Assets.Graphic;
import Assets.Directories;

import Graphic.PostProcessor;
import Graphic.ImageAtlas;
import Graphic.Draw.Func;


import ext.encode;

import std;

import Align;

import Core.Vulkan.DescriptorBuffer;
import ext.meta_programming;
import ext.object_pool;
import ext.array_queue;
import ext.array_stack;

import Assets.Graphic.PostProcess;
import Core.Bundle;

import MainTest;

import ext.stack_trace;
import ext.views;
import ext.algo;
import ext.algo.string_parse;
import ext.event;
import ext.algo.timsort;
import ext.heterogeneous;

import Graphic.Vertex;

import Font;
import Font.Manager;
import Font.TypeSettings;
import Font.TypeSettings.Renderer;

import Math.Rand;

import Core.Global;

import Game.Entity.HitBox;

import Math.Angle;

import ext.open_hash_map;

struct Quad : Game::Hitbox,  Geom::QuadTreeAdaptable<Quad, float>{
	bool collided{};

	using Game::Hitbox::Hitbox;

	[[nodiscard]] Geom::Rect_Orthogonal<float> getBound() const noexcept{
		return getMaxWrapBound();
	}

	[[nodiscard]] bool roughIntersectWith(const Quad& other) const{
		return collideWithRough(other);
	}

	[[nodiscard]] bool exactIntersectWith(const Quad& other) const{
		return collideWithExact(other) != nullptr;
	}

	[[nodiscard]] bool containsPoint(Geom::Vector2D<float>::PassType point) const{
		return contains(point);
	}
};

using QuadTreeType = Geom::quad_tree<Quad>;

template <std::size_t layers>
struct quad_tree_mdspan_trait{
	using index_type = unsigned;

	template <std::size_t>
	static constexpr std::size_t sub_size = 4;

	static consteval auto extent_impl(){
		return [] <std::size_t... I> (std::index_sequence<I...>){
			return std::extents<index_type, sub_size<I> ...>{};
		}(std::make_index_sequence<layers>());
	}

	static consteval std::size_t expected_size(){
		return [] <std::size_t... I> (std::index_sequence<I...>){
			return (sub_size<I> * ...);
		}(std::make_index_sequence<layers>());
	}

	using extent_type = decltype(extent_impl());
	static constexpr std::size_t required_span_size = expected_size();
};

using qts = quad_tree_mdspan_trait<8>;
using extent = qts::extent_type;

// constexpr auto t = qts::required_span_size;

struct T{
	void foo(int, float);
};

// std::string s{};
//
// std::pair<const std::string&, std::string&> foo(){
// 	return std::pair<const std::string&, std::string&>{s, s};
// }

int main(){
	std::unordered_map<std::string, std::string> umapP{};
	// umapP.insert_or_assign()
	ext::open_hash_map<std::string, std::string, ext::transparent::string_hasher, ext::transparent::string_equal_to> map{{}};

	using namespace std::literals;

	umapP.insert_or_assign("abc", "456");
	umapP.insert_or_assign("def", "789");

	for (auto&& [k, v] : umapP){
		v += "sb";
	}

	for (auto&& [k, v] : umapP){
		std::println("{}|{}", k, v);
	}

	map.insert_or_assign("abc", "456");
	map.insert_or_assign("def", "789");

	for (const auto& [k, v] : map){
		v += "sb";
	}

	for (const auto& [k, v] : std::as_const(map)){
		std::println("{}|{}", k, v);
	}

	std::println("{}", map.at("abc"sv));

	Geom::UniformTransform transform1{
		.vec = {0, 0},
		.rot = 30.f
	};

	Geom::UniformTransform transform2{
		.vec = {30, 30},
		.rot = 170.f
	};

	// std::pair<int, int> v{};
	// auto t = std::asc
	// const_cast<std::pair<const int, int>>(v);

	// auto c = transform1 | transform2;
	// Geom::Transform trans = c;
	//
	//
	// Math::Angle angle1{320.f};
	// Math::Angle angle2{-150.f};
	// angle2 = 30.f;
	// float rst = angle1 + angle2;

	// for(int i = 0; i < 16; ++i){
	// 	float f = i * Math::DEG_FULL / 16.f;
	//
	// 	std::println("vec2{},", Geom::dirNor(f));
	// }
	// using quad_tree_span = std::mdspan<Geom::OrthoRectFloat, extent>;
	// constexpr auto t = quad_tree_span::mapping_type::sp
	//
	// std::array<Geom::Rect_Orthogonal<float>, quad_tree_span::extents_type::()> rects{};
	// std::mdspan<Geom::OrthoRectFloat, std::extents<unsigned, 4, 4, 4, 4, 4, 4, 4, 4>> ex_mdspan{nullptr};

	return 0;
}

int main_(){
	using namespace Core;

	Global::init_context();

	Test::compileAllShaders();

	Assets::load(Global::vulkanManager->context);

	Global::init_assetsAndRenderers();

	Test::initFx();
	Test::loadTex();
	// Test::initDefUI();

	using namespace Core::Global;

	File file{R"(D:\projects\vulkan_framework\properties\resource\assets\parse_test.txt)"};
	std::shared_ptr<Font::TypeSettings::GlyphLayout> layout{new Font::TypeSettings::GlyphLayout};
	std::shared_ptr<Font::TypeSettings::GlyphLayout> fps{new Font::TypeSettings::GlyphLayout};
	layout->setAlign(Align::Pos::right);

	Font::TypeSettings::globalInstantParser.requestParseInstantly(layout, file.readString());
	// parser.taskQueue.handleAll();
	// rst->get();

	QuadTreeType quad_tree{{{-20000, -20000}, {20000, 20000}}};
	std::deque<Quad> quads{};

	{
		Math::Rand rand{};
		for(int i = 0; i < 0; ++i){
			quads.push_back(Quad{
					{
						Game::HitBoxComponent{
							.trans = {rand.range(80.f), rand.range(80.f), rand.random(360.f)},
							.box = Geom::RectBox{
								{rand.random(200.f, 700.f), rand.random(200.f, 700.f)},
								{rand.random(50.f, 80.f), rand.random(50.f, 80.f)}
							}
						},
						Game::HitBoxComponent{
							.trans = {rand.range(80.f), rand.range(80.f), rand.random(360.f)},
							.box = Geom::RectBox{
								{rand.random(200.f, 700.f), rand.random(200.f, 700.f)},
								{rand.random(50.f, 80.f), rand.random(50.f, 80.f)}
							}
						},
						Game::HitBoxComponent{
							.trans = {rand.range(80.f), rand.range(80.f), rand.random(360.f)},
							.box = Geom::RectBox{
								{rand.random(200.f, 700.f), rand.random(200.f, 700.f)},
								{rand.random(50.f, 80.f), rand.random(50.f, 80.f)}
							}
						},
					},
					{
						.vec = {rand.range(16000.f), rand.range(16000.f)},
						.rot = rand.random(360.f)
					}
				});
		}
	}

	std::uint32_t fps_count{};
	float sec{};

	timer.resetTime();

	while(!window->shouldClose()){
		timer.fetchTime();
		window->pollEvents();
		input->update(timer.globalDeltaTick());
		mainCamera->update(timer.globalDeltaTick());
		Global::mainEffectManager.dumpBuffer_unchecked();
		Global::mainEffectManager.update(timer.updateDeltaTick());

		Test::update(timer.updateDeltaTick());

		Global::UI::root->layout();
		Global::UI::root->update(timer.globalDeltaTick());
		Global::UI::renderer->resetScissors();

		if(mainCamera->checkChanged()){
			rendererWorld->updateProjection(Vulkan::UniformProjectionBlock{mainCamera->getWorldToScreen(), 0.f});
			rendererWorld->updateCameraProperties(Graphic::CameraProperties{
					.scale = mainCamera->getScale()
				});
		}

		constexpr auto baseColor = Graphic::Colors::WHITE;
		constexpr auto lightColor = Graphic::Colors::CLEAR.copy().appendLightColor(Graphic::Colors::WHITE);

		Geom::Vec2 off{timer.getGlobalTime() * 2.f + 0, 0};
		Geom::Vec2 off2 = off.copy().add(50, 50);


		{
			if(!timer.isPaused()){
				quad_tree.reserved_clear();

				Math::Rand rand{10};
				for(auto& value : quads){
					value.collided = false;
					value.move(Geom::Vec2{rand.range(5.f), rand.range(5.f)}  * timer.updateDeltaTick());
					quad_tree.insert(value);
					value.clearCollided();
				}

				// for (auto & value : quads){
				// 	if(quad_tree.intersect_any(value)){
				// 		value.collided = true;
				// 	}
				// }

				std::for_each(std::execution::par, quads.begin(), quads.end(), [&](Quad& value){
					// if(!mainCamera->getViewport().overlap_Exclusive(value.val))return;
					if(quad_tree.intersect_any(value)){
						value.collided = true;
					}
				});
			}


		}

		// Font::TypeSettings::globalInstantParser.requestParseInstantly(fps, std::format("#<font|tele>{:5}|", quad_tree.size()));

		sec += timer.getGlobalDelta();
		if(sec > 1.f){
			sec = 0;
			Font::TypeSettings::globalInstantParser.requestParseInstantly(fps, std::format("#<font|tele>{}", fps_count));
			fps_count = 0;
		}
		fps_count++;

		// Font::TypeSettings::draw(UI::renderer->batch, fps, {300, 300});

		if constexpr(true){
			Global::mainEffectManager.render(mainCamera->getViewport());

			Graphic::InstantBatchAutoParam<Graphic::Vertex_World, Graphic::Draw::DepthModifier> autoParam{rendererWorld->batch};

			Geom::Vec2 size{800, 800};


			autoParam.modifier.depth = 0.5f;
			autoParam << Test::texturePester;
			Graphic::Draw::Drawer<Graphic::Vertex_World>::rect(
				++autoParam,
				{{400, 200}, 100},
				size, baseColor);

			autoParam << Test::texturePesterLight;
			Graphic::Draw::Drawer<Graphic::Vertex_World>::rect(
				++autoParam,
				{{400, 200}, 100},
				size, lightColor);


			autoParam.modifier.depth = 0.3f;
			autoParam << Test::texturePester;
			Graphic::Draw::Drawer<Graphic::Vertex_World>::rect(
				++autoParam,
				{{460, 240}, 45},
				size, baseColor);

			autoParam << Test::texturePesterLight;
			Graphic::Draw::Drawer<Graphic::Vertex_World>::rect(
				++autoParam,
				{{460, 240}, 45},
				size, lightColor);

			autoParam << Graphic::Draw::WhiteRegion;
			quad_tree.each([&](const QuadTreeType& node){
				if(mainCamera->getViewport().overlap_Exclusive(node.get_boundary())){
					auto color = Graphic::Colors::ROYAL;

					autoParam.modifier.depth = 0.35f;
					Graphic::Draw::Drawer<Graphic::Vertex_World>::Line::rectOrtho(autoParam, 4.f, node.get_boundary().shrink(8), color);
				}

				for(Quad* item : node.get_items()){
					if(mainCamera->getViewport().overlap_Exclusive(item->getBound())){
						auto color = Graphic::Colors::CLEAR;
						if(item->collided){
							color.appendLightColor(Graphic::Colors::RED_DUSK);
						}else{
							color.appendLightColor(Graphic::Colors::PALE_GREEN);
						}

						autoParam.modifier.depth = 0.0f;
						for (const auto & component : item->components){
							Graphic::Draw::Drawer<Graphic::Vertex_World>::Line::circularPoly(autoParam, 4.f, component.box, 4, color);
						Graphic::Draw::Drawer<Graphic::Vertex_World>::Line::line(++autoParam, 4.f, item->trans.vec, component.box.transform.vec, Graphic::Colors::ROYAL.copy().toLightColor(), Graphic::Colors::YELLOW.copy().toLightColor());
						}

						Graphic::Draw::Drawer<Graphic::Vertex_World>::Line::line(++autoParam, 4.f, item->trans.vec, node.get_boundary().getCenter(), color, Graphic::Colors::ROYAL.copy().toLightColor());
					}
				}
			});

			Global::rendererWorld->batch.consumeAll();
			Global::rendererWorld->doPostProcess();
		}

		// Global::UI::root->draw();
		// Global::UI::root->draw();
		// Global::UI::root->draw();
		// Global::UI::root->draw();
		// Global::UI::root->draw();
		// Global::UI::root->draw();
		// Global::UI::root->draw();

		Graphic::InstantBatchAutoParam<Graphic::Vertex_UI> param{
				Global::UI::renderer->batch, Graphic::Draw::WhiteRegion
			};

		Geom::grid_generator<4, float> generator{};
		for(auto rects : Geom::deferred_grid_generator{
			    std::array{
				    Geom::Vec2{50.f, 80.f}, Geom::Vec2{120.f, 160.f}, Geom::Vec2{250.f, 320.f}, Geom::Vec2{400.f, 400.f}, Geom::Vec2{600.f, 600.f},
				    Geom::Vec2{700.f, 650.f}
			    }
		    }){
			Graphic::Draw::Drawer<Graphic::Vertex_UI>::Line::rectOrtho(param, 1.f, rects.move(200, 300), Graphic::Colors::AQUA);
		}

		Geom::OrthoRectFloat rect{Geom::FromExtent, {20, 80}, {600, 800}};

		// param << imageAtlas.find("main.frame");
		//
		// Graphic::Draw::Drawer<Vulkan::Vertex_UI>::rectOrtho(++param, rect, Graphic::Colors::AQUA);
		Graphic::Draw::drawNinePatch(param, Test::nineRegion_edge, rect, Graphic::Colors::AQUA);

		// Font::TypeSettings::draw(Global::UI::renderer->batch, layout, {200 + timer.getGlobalTime() * 5.f, 200});

		// Global::UI::renderer->pushScissor({200, 200, 1200, 700}, false);
		// Global::UI::renderer->blit();

		// Font::TypeSettings::draw(Global::UI::renderer->batch, layout, {100, 100});


		// Global::UI::renderer->blit();

		Font::TypeSettings::draw(Global::UI::renderer->batch, fps, {200 + timer.getGlobalTime() * 5.f, 200});

		Global::UI::renderer->batch.consumeAll();
		Global::UI::renderer->blit();

		vulkanManager->mergePresent();
		Global::UI::renderer->clearMerged();

		vulkanManager->blitToScreen();
	}


	vkDeviceWaitIdle(vulkanManager->context.device);

	Test::texturePester = {};
	Test::texturePesterLight = {};

	Assets::dispose();

	Core::Global::terminate();

	return 0; //just for main func test swap...
}

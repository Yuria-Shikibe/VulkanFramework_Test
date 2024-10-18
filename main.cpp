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

int main_(){
	return 0;
}

int main(){
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

	{
		Math::Rand rand{};
		for(int i = 0; i < 100; ++i){
			Test::GamePart::world.add([&rand](Game::RealEntity& entity){
				entity.motion.trans = {
						.vec = {rand.range(16000.f), rand.range(16000.f)},
						.rot = rand.random(360.f)
					};
				entity.hitbox = Game::Hitbox{{
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
					}};
			});
		}
	}

	std::uint32_t fps_count{};
	float sec{};

	Test::GamePart::init();

	timer.resetTime();

	while(!window->shouldClose()){
		timer.fetchTime();
		window->pollEvents();
		input->update(timer.globalDeltaTick());
		mainCamera->update(timer.globalDeltaTick());
		Global::mainEffectManager.dumpBuffer_unchecked();
		Global::mainEffectManager.update(timer.updateDeltaTick());

		Test::update(timer.updateDeltaTick());
		Test::GamePart::postUpdate();

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

				Math::Rand rand{10};
				for(auto& value : Test::GamePart::world.realEntities.getEntities()){
					value.motion.trans.vec += Geom::dirNor(rand.random(360.f)) * (rand.random(1.f, 200.f) * timer.updateDeltaTick());
				}
			}

		}

		Test::GamePart::update(timer.getUpdateDelta());

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

			Test::GamePart::draw();

			using Drawer = Graphic::Draw::Drawer<Graphic::Vertex_World>;
			Graphic::InstantBatchAutoParam<Graphic::Vertex_World, Graphic::Draw::DepthModifier> autoParam{rendererWorld->batch};

			Geom::Vec2 size{800, 800};


			autoParam.modifier.depth = 0.5f;
			autoParam << Test::texturePester;
			Drawer::rect(
				++autoParam,
				{{400, 200}, 100},
				size, baseColor);

			autoParam << Test::texturePesterLight;
			Drawer::rect(
				++autoParam,
				{{400, 200}, 100},
				size, lightColor);


			autoParam.modifier.depth = 0.3f;
			autoParam << Test::texturePester;
			Drawer::rect(
				++autoParam,
				{{460, 240}, 45},
				size, baseColor);

			autoParam << Test::texturePesterLight;
			Drawer::rect(
				++autoParam,
				{{460, 240}, 45},
				size, lightColor);

			autoParam << Graphic::Draw::WhiteRegion;
			Test::GamePart::world.quadTree.each([&](const Geom::quad_tree<Game::RealEntity>& node){
				if(mainCamera->getViewport().overlap_Exclusive(node.get_boundary())){
					auto color = Graphic::Colors::ROYAL;

					autoParam.modifier.depth = 0.35f;
					Drawer::Line::rectOrtho(autoParam, 4.f, node.get_boundary().shrink(8), color);
				}

				for(Game::RealEntity* item : node.get_items()){
					if(mainCamera->getViewport().overlap_Exclusive(item->getBound())){
						auto color = Graphic::Colors::CLEAR;
						if(!item->intersections.collisions.empty()){
							color.appendLightColor(Graphic::Colors::RED_DUSK);
						}else{
							color.appendLightColor(Graphic::Colors::PALE_GREEN);
						}

						autoParam.modifier.depth = 0.0f;
						for (const auto & component : item->hitbox.components){
							Drawer::Line::circularPoly_fixed<4>(autoParam, 4.f, component.box, color);
							Drawer::Line::line(++autoParam, 4.f, item->motion.trans.vec, component.box.transform.vec, Graphic::Colors::ROYAL.copy().toLightColor(), Graphic::Colors::YELLOW.copy().toLightColor());
						}

						Drawer::Line::circularPoly_fixed<4>(autoParam, 4.f, item->hitbox.getWrapBox(), color);
						Drawer::Line::rectOrtho(autoParam, 4.f, item->hitbox.getMinWrapBound(), color);
						Drawer::Line::line(++autoParam, 4.f, item->motion.trans.vec, node.get_boundary().getCenter(), color, Graphic::Colors::ROYAL.copy().toLightColor());
					}
				}
			});

			Global::rendererWorld->batch.consumeAll();
			Global::rendererWorld->doPostProcess();
		}

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

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

// #include "src/application_head.h"

import Core.Window;
import Core.Vulkan.Manager;
import Core.Vulkan.Instance;
import Core.Vulkan.Vertex;
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
import Assets.Bundle;

import MainTest;

import ext.stack_trace;
import ext.views;
import ext.algo;
import ext.algo.string_parse;
import ext.event;
import ext.algo.timsort;
import ext.heterogeneous;

import Core.Vulkan.Vertex;

import Font;
import Font.Manager;
import Font.TypeSettings;
import Font.TypeSettings.Renderer;

import Math.Rand;

import Core.Global;


int main(){
    using namespace Core;

	Global::init_context();

    Test::compileAllShaders();

	Assets::load(Global::vulkanManager->context);

    Global::init_assetsAndRenderers();

    using namespace Core::Global;

    File file{R"(D:\projects\vulkan_framework\properties\resource\assets\parse_test.txt)"};
    Test::loadTex();

    Test::initDefUI();


    std::shared_ptr<Font::TypeSettings::GlyphLayout> layout{new Font::TypeSettings::GlyphLayout};
    std::shared_ptr<Font::TypeSettings::GlyphLayout> fps{new Font::TypeSettings::GlyphLayout};
    layout->setAlign(Align::Pos::right);

    Font::TypeSettings::globalInstantParser.requestParseInstantly(layout, file.readString());
    // parser.taskQueue.handleAll();
    // rst->get();

	Test::texturePester = Vulkan::Texture{vulkanManager->context.physicalDevice, vulkanManager->context.device};
	Test::texturePester.loadPixmap(vulkanManager->obtainTransientCommand(), Assets::Dir::texture / R"(pester.png)");

	Test::texturePesterLight = Vulkan::Texture{vulkanManager->context.physicalDevice, vulkanManager->context.device};
	Test::texturePesterLight.loadPixmap(vulkanManager->obtainTransientCommand(), Assets::Dir::texture / R"(pester.light.png)");

    std::uint32_t fps_count{};
    float sec{};

    timer.resetTime();

    while(!window->shouldClose()) {
        timer.fetchTime();
        window->pollEvents();
        input->update(timer.globalDeltaTick());
        mainCamera->update(timer.globalDeltaTick());
        Global::UI::root->layout();
        Global::UI::root->update(timer.globalDeltaTick());

        if(mainCamera->checkChanged()){
            rendererWorld->updateProjection(Vulkan::UniformProjectionBlock{mainCamera->getWorldToScreen(), 0.f});
            rendererWorld->updateCameraProperties(Graphic::CameraProperties{
                .scale = mainCamera->getScale()
            });
        }


        constexpr auto baseColor = Graphic::Colors::WHITE;
        constexpr auto lightColor = Graphic::Colors::CLEAR.copy().appendLightColor(Graphic::Colors::WHITE);

        Geom::Vec2 off{ timer.getGlobalTime() * 5.f + 0, 0};
        Geom::Vec2 off2 = off.copy().add(50, 50);

        sec += timer.getGlobalDelta();
        if(sec > 1.f){
            sec = 0;
            Font::TypeSettings::globalInstantParser.requestParseInstantly(fps, std::format("#<font|tele>{}", fps_count));
            fps_count = 0;
        }
        fps_count++;
        // Font::TypeSettings::draw(UI::renderer->batch, fps, {300, 300});
        Global::UI::renderer->resetScissors();


        if constexpr (false){
            {
                auto [imageIndex, s, dataPtr, captureLock] = rendererWorld->batch.acquire(Test::texturePester.getView());
                new(dataPtr) std::array{
                    Vulkan::Vertex_World{Geom::Vec2{0  , 0  }.add(off), 0.7f, {imageIndex}, baseColor, {0.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 0  }.add(off), 0.7f, {imageIndex}, baseColor, {1.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 800}.add(off), 0.7f, {imageIndex}, baseColor, {1.0f, 0.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{0  , 800}.add(off), 0.7f, {imageIndex}, baseColor, {0.0f, 0.0f}},
                };
            }

            {
                auto [imageIndex, s, dataPtr, captureLock] = rendererWorld->batch.acquire(Test::texturePesterLight.getView());
                new(dataPtr) std::array{
                    Vulkan::Vertex_World{Geom::Vec2{0  , 0  }.add(off), 0.7f, {imageIndex}, lightColor, {0.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 0  }.add(off), 0.7f, {imageIndex}, lightColor, {1.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 800}.add(off), 0.7f, {imageIndex}, lightColor, {1.0f, 0.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{0  , 800}.add(off), 0.7f, {imageIndex}, lightColor, {0.0f, 0.0f}},
                };
            }

            {
                auto [imageIndex, s, dataPtr, captureLock] = rendererWorld->batch.acquire(Test::texturePester.getView());
                new(dataPtr) std::array{
                    Vulkan::Vertex_World{Geom::Vec2{0  , 0  }.add(off2), 0.5f, {imageIndex}, baseColor, {0.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 0  }.add(off2), 0.5f, {imageIndex}, baseColor, {1.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 800}.add(off2), 0.5f, {imageIndex}, baseColor, {1.0f, 0.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{0  , 800}.add(off2), 0.5f, {imageIndex}, baseColor, {0.0f, 0.0f}},
                };
            }

            {
                auto [imageIndex, s, dataPtr, captureLock] = rendererWorld->batch.acquire(Test::texturePesterLight.getView());
                new(dataPtr) std::array{
                    Vulkan::Vertex_World{Geom::Vec2{0  , 0  }.add(off2), 0.5f, {imageIndex}, lightColor, {0.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 0  }.add(off2), 0.5f, {imageIndex}, lightColor, {1.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 800}.add(off2), 0.5f, {imageIndex}, lightColor, {1.0f, 0.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{0  , 800}.add(off2), 0.5f, {imageIndex}, lightColor, {0.0f, 0.0f}},
                };
            }

            Global::rendererWorld->batch.consumeAll();
            Global::rendererWorld->doPostProcess();
        }

        Global::UI::root->draw();

        Graphic::InstantBatchAutoParam<Vulkan::Vertex_UI> param{
            Global::UI::renderer->batch, Graphic::Draw::WhiteRegion
        };

        Geom::grid_generator<4, float> generator{};
        for (auto rects : Geom::deferred_grid_generator{
            std::array{Geom::Vec2{50.f, 80.f}, Geom::Vec2{120.f, 160.f}, Geom::Vec2{250.f, 320.f}, Geom::Vec2{400.f, 400.f}, Geom::Vec2{600.f, 600.f}, Geom::Vec2{700.f, 650.f}}}){
            Graphic::Draw::Drawer<Vulkan::Vertex_UI>::Line::rectOrtho(param, 1.f, rects.move(200, 300), Graphic::Colors::AQUA);
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

	return 0;//just for main func test swap...
}

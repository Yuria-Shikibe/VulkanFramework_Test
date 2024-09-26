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

import Graphic.Renderer.UI;
import Graphic.Renderer.World;
import Core.Global;
import Core.Global.Graphic;
import Core.Global.UI;
import Core.UI.Root;

import Font;
import Font.GlyphToRegion;
import Font.TypeSettings;
import Font.TypeSettings.Renderer;

import Math.Rand;

Core::Vulkan::Texture texturePester{};
Core::Vulkan::Texture texturePesterLight{};

Graphic::ImageAtlas loadTex(){

    std::unordered_map<std::string, std::uint64_t> tags{};

    using namespace Core;
    Graphic::ImageAtlas imageAtlas{Global::vulkanManager->context};

    auto& mainPage = imageAtlas.registerPage("main");
    auto& fontPage = imageAtlas.registerPage("font");

    auto& page = fontPage.createPage(imageAtlas.context);

    page.texture.cmdClearColor(imageAtlas.obtainTransientCommand(), {1., 1., 1., 0.});

    auto region = imageAtlas.allocate(mainPage, Graphic::Pixmap{Assets::Dir::texture.find("white.png")});
    region.shrink(15);
    Graphic::Draw::WhiteRegion = &mainPage.registerNamedRegion("white", std::move(region)).first;

    return imageAtlas;
}

Font::FontManager initFontManager(Graphic::ImageAtlas& atlas){
    Font::FontManager fontManager{atlas};

    //must with NRVO
    Font::GlobalFontManager = &fontManager;

    std::vector<std::pair<std::string_view, std::string_view>> font{
        {"SourceHanSerifSC-SemiBold.otf", "srcH"},
        {"telegrama.otf", "tele"}
    };

    for (const auto & [name, key] : font){
        Font::FontFaceStorage fontStorage{Assets::Dir::font.subFile(name).absolutePath().string().c_str()};
        Font::FontGlyphLoader loader{
            .storage = fontStorage,
            .size = {0, 48},
            .sections = {{std::from_range, Font::ASCII_CHARS}}
        };

        auto id = fontManager.registerFace(std::move(fontStorage));
        Font::namedFonts.insert_or_assign(key, id);
    }

    return fontManager;
}

struct TestT{
    int v1{};
    int rank{};
};

int main_(){
    ext::projection_priority_queue<decltype(&TestT::rank), std::vector<TestT>, std::greater<>>
        queue{&TestT::rank};

    queue.push({666, 57457686});
    queue.push({514, 2});
    queue.push({114, 1});
    queue.push({666, 3});
    queue.push({666, 151253});
    queue.push({12390123, 0});

    while(!queue.empty()){
        std::println("{}", queue.top().rank);
        queue.pop();
    }

    // using namespace std::literals;
    // auto gen = std::mt19937{std::random_device{}()};
    // gen.seed(10);
    // for(int i = 0; i < 10; ++i){
    //     std::println("{}", gen());
    // }
    // Math::Rand rand{};
    //
    // std::vector<std::size_t> arr{};
    //
    // for(std::size_t i = 0; i < 100000000ull; ++i){
    //     arr.push_back(rand.nextLong(100000000ull));
    // }
    //
    // std::cout << "genDone" << std::endl;
    //
    // {
    //     auto test = arr;
    //     auto begin = std::chrono::high_resolution_clock::now();
    //     ext::algo::timsort(test);
    //     auto end = std::chrono::high_resolution_clock::now();
    //
    //     std::println("{}, {}", std::ranges::is_sorted(test), end - begin);
    // }
    //
    // std::cout << "tim sort done" << std::endl;
    //
    // {
    //     auto test = arr;
    //     auto begin = std::chrono::high_resolution_clock::now();
    //     std::ranges::sort(test);
    //     auto end = std::chrono::high_resolution_clock::now();
    //
    //     std::println("{}, {}", std::ranges::is_sorted(test), end - begin);
    // }
    //
    // std::cout << "std sort done" << std::endl;

    return 0;
}

int main(){
    using namespace Core;

	Global::init();

    Test::compileAllShaders();

	Assets::load(Global::vulkanManager->context);

    Global::postInit();

    using namespace Core::Global;

    File file{R"(D:\projects\vulkan_framework\properties\resource\assets\parse_test.txt)"};
    auto imageAtlas = loadTex();

    Font::FontManager fontManager = initFontManager(imageAtlas);

    Test::initDefUI();


    std::shared_ptr<Font::TypeSettings::GlyphLayout> layout{new Font::TypeSettings::GlyphLayout};
    std::shared_ptr<Font::TypeSettings::GlyphLayout> fps{new Font::TypeSettings::GlyphLayout};
    layout->setAlign(Align::Pos::right);

    Font::TypeSettings::globalInstantParser.requestParseInstantly(layout, file.readString());
    // parser.taskQueue.handleAll();
    // rst->get();

	texturePester = Vulkan::Texture{vulkanManager->context.physicalDevice, vulkanManager->context.device};
	texturePester.loadPixmap(vulkanManager->obtainTransientCommand(), Assets::Dir::texture / R"(pester.png)");

	texturePesterLight = Vulkan::Texture{vulkanManager->context.physicalDevice, vulkanManager->context.device};
	texturePesterLight.loadPixmap(vulkanManager->obtainTransientCommand(), Assets::Dir::texture / R"(pester.light.png)");

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
                auto [imageIndex, s, dataPtr, captureLock] = rendererWorld->batch.acquire(texturePester.getView());
                new(dataPtr) std::array{
                    Vulkan::Vertex_World{Geom::Vec2{0  , 0  }.add(off), 0.7f, {imageIndex}, baseColor, {0.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 0  }.add(off), 0.7f, {imageIndex}, baseColor, {1.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 800}.add(off), 0.7f, {imageIndex}, baseColor, {1.0f, 0.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{0  , 800}.add(off), 0.7f, {imageIndex}, baseColor, {0.0f, 0.0f}},
                };
            }

            {
                auto [imageIndex, s, dataPtr, captureLock] = rendererWorld->batch.acquire(texturePesterLight.getView());
                new(dataPtr) std::array{
                    Vulkan::Vertex_World{Geom::Vec2{0  , 0  }.add(off), 0.7f, {imageIndex}, lightColor, {0.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 0  }.add(off), 0.7f, {imageIndex}, lightColor, {1.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 800}.add(off), 0.7f, {imageIndex}, lightColor, {1.0f, 0.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{0  , 800}.add(off), 0.7f, {imageIndex}, lightColor, {0.0f, 0.0f}},
                };
            }

            {
                auto [imageIndex, s, dataPtr, captureLock] = rendererWorld->batch.acquire(texturePester.getView());
                new(dataPtr) std::array{
                    Vulkan::Vertex_World{Geom::Vec2{0  , 0  }.add(off2), 0.5f, {imageIndex}, baseColor, {0.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 0  }.add(off2), 0.5f, {imageIndex}, baseColor, {1.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 800}.add(off2), 0.5f, {imageIndex}, baseColor, {1.0f, 0.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{0  , 800}.add(off2), 0.5f, {imageIndex}, baseColor, {0.0f, 0.0f}},
                };
            }

            {
                auto [imageIndex, s, dataPtr, captureLock] = rendererWorld->batch.acquire(texturePesterLight.getView());
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
    // gcm = {};


    imageAtlas = {};
    fontManager = {};
    texturePester = {};
	texturePesterLight = {};

	Assets::dispose();

	Core::Global::terminate();

	return 0;//just for main func test swap...
}

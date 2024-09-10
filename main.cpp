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
import Graphic.Batch;
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

import Font;
import Font.GlyphToRegion;
import Font.TypeSettings;
import Font.TypeSettings.Renderer;
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
import ext.algo.string_parse;
import ext.Event;

import Core.Vulkan.Vertex;

import Graphic.Renderer.UI;
import Graphic.Renderer.World;
import Core.Global;


Core::Vulkan::Texture texturePester{};
Core::Vulkan::Texture texturePesterLight{};

Graphic::ImageAtlas loadTex(){
    using namespace Core;
    Graphic::ImageAtlas imageAtlas{vulkanManager->context};

    auto& mainPage = imageAtlas.registerPage("main");
    auto& fontPage = imageAtlas.registerPage("font");

    auto& page = fontPage.createPage(imageAtlas.context);

    page.texture.cmdClearColor(imageAtlas.obtainTransientCommand(), {0., 1., 1., 1.});

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


struct T1{
    virtual ~T1() = default;

    virtual void print() const{
        std::println("T1");
    }
};

struct T2 : T1{
    void print() const override{
        std::println("T2");
    }
};

void foo(){
    ext::event_submitter<true> submitter_pmr{
        ext::index_of<T1>(), ext::index_of<T2>()};

    T2 t{};

    submitter_pmr.submit<T1>(&t);
}

int main_(){
    foo();

    return 0;
}

int main(){
    using namespace Core;

	::Core::init();

    Test::compileAllShaders();

	Assets::load(vulkanManager->context);

    ::Core::postInit();


    File file{R"(D:\projects\vulkan_framework\properties\resource\assets\parse_test.txt)"};
    auto imageAtlas = loadTex();

    Font::FontManager fontManager = initFontManager(imageAtlas);

    std::shared_ptr<Font::TypeSettings::Layout> layout{new Font::TypeSettings::Layout};
    std::shared_ptr<Font::TypeSettings::Layout> fps{new Font::TypeSettings::Layout};
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
        timer.fetchApplicationTime();
        window->pollEvents();
        input->update(timer.globalDeltaTick());
        mainCamera->update(timer.globalDeltaTick());

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
        // Font::TypeSettings::draw(rendererUI->batch, fps, {300, 300});



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

        rendererWorld->batch.consumeAll();
        rendererWorld->doPostProcess();

        Font::TypeSettings::draw(rendererUI->batch, layout, {200 + timer.getGlobalTime() * 5.f, 200});

        rendererUI->pushScissor({200, 200, 1200, 1200}, false);
        rendererUI->blit();

        Font::TypeSettings::draw(rendererUI->batch, layout, {100, 100});

        rendererUI->resetScissors();
        rendererUI->blit();

        Font::TypeSettings::draw(rendererUI->batch, fps, {200 + timer.getGlobalTime() * 5.f, 200});

        rendererUI->batch.consumeAll();
        rendererUI->blit();

        rendererWorld->doMerge();

        vulkanManager->blitToScreen();

        rendererUI->clearAll();
    }


    vkDeviceWaitIdle(vulkanManager->context.device);
    // gcm = {};


    imageAtlas = {};
    fontManager = {};
    texturePester = {};
	texturePesterLight = {};

	Assets::dispose();

	terminate();

	return 0;//just for main func test swap...
}

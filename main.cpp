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


import Core.File;
import Graphic.Color;
import Graphic.Batch;
import Graphic.Pixmap;

import Core.Input;
import Graphic.Camera2D;
import Core.InitAndTerminate;
import Core.Global;

import Geom.Matrix4D;
import Geom.Rect_Orthogonal;

import Core.Vulkan.Shader.Compile;

import Assets.Graphic;
import Assets.Directories;

Core::Vulkan::Texture texturePester{};
Core::Vulkan::Texture texturePesterLight{};

import Graphic.RendererUI;
import Graphic.PostProcessor;
import Graphic.ImageAtlas;
import Graphic.Draw.Context;

import Font;
import Font.GlyphToRegion;
import Font.TypeSettings;
import Font.TypeSettings.Renderer;
import ext.Encoding;

import std;

import Align;

import ext.MetaProgramming;

import Assets.Graphic.PostProcess;
import Assets.Bundle;

template <typename Rng>
concept Printable = requires(const std::ranges::range_const_reference_t<Rng>& rng, std::ostream& ostream){
    requires std::ranges::input_range<Rng>;
    { ostream << rng } -> std::same_as<std::ostream&>;
};

static_assert(Printable<std::vector<std::string>>);

Graphic::ImageAtlas loadTex(){
    using namespace Core;
    Graphic::ImageAtlas imageAtlas{vulkanManager->context};

    // File file{R"(D:\projects\NewHorizonMod\assets)"};
    //
    auto& mainPage = imageAtlas.registerPage("main");
    auto& fontPage = imageAtlas.registerPage("font");

    auto& page = fontPage.createPage(imageAtlas.context);

    // Core::Vulkan::Util::
    page.texture.cmdClearColor(imageAtlas.obtainTransientCommand(), {1., 1., 1., 0.});

    auto region = imageAtlas.allocate(mainPage, Graphic::Pixmap{Assets::Dir::texture.find("white.png")});
    region.shrink(15);
    Graphic::Draw::WhiteRegion = &mainPage.registerNamedRegion("white", std::move(region)).first;

    return imageAtlas;
}

void compileAllShaders() {
    const Core::Vulkan::ShaderRuntimeCompiler compiler{};
    const Core::Vulkan::ShaderCompilerWriter adaptor{compiler, Assets::Dir::shader_spv};

    Core::File{Assets::Dir::shader_src}.forSubs([&](Core::File&& file){
        adaptor.compile(file);
    });
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



    // Core::File file = Assets::Dir::assets.find("test.txt");
    //
    // for (char32_t convertCharToChar32 : ext::encode::convertCharToChar32(file.readString())){
    //     fontManager.getGlyph(id, {convertCharToChar32, {0, 120}});
    // }

    return fontManager;
}


// int main(){
//     Core::initFileSystem();
//     Core::Bundle bundle{Assets::Dir::bundle.subFile("bundle.zh_cn.json"), Assets::Dir::bundle.subFile("bundle.def.json")};
//
//
//     std::println("{}", bundle.getLocale().name());
//
//     std::println("{}", bundle["control-bind.group.basic-group"]);
//     std::println("{}", bundle[{"control-bind", "group", "basic-group"}]);
//     std::println("{}", bundle[{}]);
//
// }

int main(){
    using namespace Core;

	init();

    compileAllShaders();

	Assets::load(vulkanManager->context);


    Graphic::RendererUI* rendererUi = new Graphic::RendererUI{vulkanManager->context};


    auto* batch = new Graphic::Batch{vulkanManager->context, sizeof(Vulkan::Vertex_World)};
    vulkanManager->batchVertexData = batch->getBatchData();


    vulkanManager->registerResizeCallback(
        [rendererUi](auto& e){
            if(e.area()) rendererUi->resize(static_cast<Geom::USize2>(e));
        }, [&rendererUi](auto e){
            rendererUi->initRenderPass(e);
        });

    // auto gcm = Assets::PostProcess::Factory::gaussianFactory.generate(vulkanManager->swapChain.size2D(), [&]{
    //     Graphic::AttachmentPort port{};
    //     port.in.insert_or_assign(0, rendererUi->mergeFrameBuffer.localAttachments.front().getView());
    //     port.toTransferOwnership.insert_or_assign(0, rendererUi->mergeFrameBuffer.localAttachments.front().getImage());
    //     return port;
    // });


    // vulkanManager->registerResizeCallback(
    //     [&gcm](auto& e){
    //         if(e.area()) gcm.resize(static_cast<Geom::USize2>(e));
    //     }, [&gcm](auto e){
    //         // gcm.resize(e);
    //     });

    vulkanManager->uiImageViewProv = [&]{
        return std::make_pair(rendererUi->mergeFrameBuffer.at(3), rendererUi->mergeFrameBuffer.at(4));
    };

    vulkanManager->initPipeline();


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


    batch->externalDrawCall = [](const Graphic::Batch& b){
        vulkanManager->drawFrame(b.drawCommandFence);
    };

    batch->descriptorChangedCallback = [](const Graphic::Batch& b, std::span<const VkImageView> data){
        vulkanManager->updateBatchDescriptorSet(data, &b.drawCommandFence);
        vulkanManager->createBatchDrawCommands();
    };

    std::uint32_t fps_count{};
    float sec{};

    timer.resetTime();

    while(!window->shouldClose()) {
        timer.fetchApplicationTime();
        window->pollEvents();
        input->update(timer.globalDeltaTick());
        mainCamera->update(timer.globalDeltaTick());

        if(mainCamera->checkChanged()){
            vulkanManager->updateBatchUniformBuffer(Vulkan::UniformBlock{mainCamera->getWorldToScreen(), 0.f});
            vulkanManager->updateCameraScale(mainCamera->getScale());
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

         {
            auto [imageIndex, dataPtr, captureLock] = batch->acquire(texturePester.getView());
            new(dataPtr) std::array{
                    Vulkan::Vertex_World{Geom::Vec2{0  , 0  }.add(off), 0.5f, {imageIndex}, baseColor, {0.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 0  }.add(off), 0.5f, {imageIndex}, baseColor, {1.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 800}.add(off), 0.5f, {imageIndex}, baseColor, {1.0f, 0.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{0  , 800}.add(off), 0.5f, {imageIndex}, baseColor, {0.0f, 0.0f}},
                };
        }

        {
            auto [imageIndex, dataPtr, captureLock] = batch->acquire(texturePesterLight.getView());
            new(dataPtr) std::array{
                    Vulkan::Vertex_World{Geom::Vec2{0  , 0  }.add(off), 0.5f, {imageIndex}, lightColor, {0.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 0  }.add(off), 0.5f, {imageIndex}, lightColor, {1.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 800}.add(off), 0.5f, {imageIndex}, lightColor, {1.0f, 0.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{0  , 800}.add(off), 0.5f, {imageIndex}, lightColor, {0.0f, 0.0f}},
                };
        }

        {
            auto [imageIndex, dataPtr, captureLock] = batch->acquire(texturePester.getView());
            new(dataPtr) std::array{
                    Vulkan::Vertex_World{Geom::Vec2{0  , 0  }.add(off2), 0.7f, {imageIndex}, baseColor, {0.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 0  }.add(off2), 0.7f, {imageIndex}, baseColor, {1.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 800}.add(off2), 0.7f, {imageIndex}, baseColor, {1.0f, 0.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{0  , 800}.add(off2), 0.7f, {imageIndex}, baseColor, {0.0f, 0.0f}},
                };
        }

        {
            auto [imageIndex, dataPtr, captureLock] = batch->acquire(texturePesterLight.getView());
            new(dataPtr) std::array{
                    Vulkan::Vertex_World{Geom::Vec2{0  , 0  }.add(off2), 0.7f, {imageIndex}, lightColor, {0.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 0  }.add(off2), 0.7f, {imageIndex}, lightColor, {1.0f, 1.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{800, 800}.add(off2), 0.7f, {imageIndex}, lightColor, {1.0f, 0.0f}},
                    Vulkan::Vertex_World{Geom::Vec2{0  , 800}.add(off2), 0.7f, {imageIndex}, lightColor, {0.0f, 0.0f}},
                };
        }

        batch->consumeAll();


        // rendererUi->pushScissor({100, 100, 500, 500});
        Font::TypeSettings::draw(rendererUi->batch, layout, {200, 200});
        Font::TypeSettings::draw(rendererUi->batch, fps, {300, 300});
        // rendererUi->pushScissor({200, 200, 1200, 1200}, false);
        Font::TypeSettings::draw(rendererUi->batch, layout, {100, 100});
        rendererUi->batch.consumeAll();

        rendererUi->blit();
        rendererUi->endBlit();

        // gcm.submitCommand();

        vulkanManager->blitToScreen();

        rendererUi->clear();
    }


    vkDeviceWaitIdle(vulkanManager->context.device);
    // gcm = {};


    imageAtlas = {};
    delete batch;
    delete rendererUi;

    fontManager = {};
    texturePester = {};
	texturePesterLight = {};

	Assets::dispose();

	terminate();

	return 0;//just for main func test swap...
}

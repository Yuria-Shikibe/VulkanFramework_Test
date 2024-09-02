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

Font::TypeSettings::Parser parser{};

template <typename Rng>
concept Printable = requires(const std::ranges::range_const_reference_t<Rng>& rng, std::ostream& ostream){
    requires std::ranges::input_range<Rng>;
    { ostream << rng } -> std::same_as<std::ostream&>;
};

static_assert(Printable<std::vector<std::string>>);

void loadParser(){
    using namespace Font::TypeSettings;

    parser.modifiers["size"] = [](const std::vector<std::string_view>& args, Context& context, const std::shared_ptr<Layout>& target){
        if(args.empty()){
            context.sizeHistory.pop();
            return;
        }

        if(args.front().starts_with('[')){
            const auto size = Func::string_cast_seq<std::uint16_t>(args.front().substr(1), 0, 2);

            switch(size.size()){
                case 1: context.sizeHistory.push({0, size[0]}); break;
                case 2: context.sizeHistory.push({size[0], size[1]}); break;
                default: context.sizeHistory.pop();
            }
        }
    };

    parser.modifiers["scl"] = [](const std::vector<std::string_view>& args, Context& context, const std::shared_ptr<Layout>& target){
        if(args.empty()){
            context.sizeHistory.pop();
            return;
        }

        const auto scale = Func::string_cast<float>(args.front());
        context.sizeHistory.push(context.getLastSize().scl(scale));
    };

    parser.modifiers["s"] = parser.modifiers["size"];

    parser.modifiers["color"] = [](const std::vector<std::string_view>& args, Context& context, const std::shared_ptr<Layout>& target){
        if(args.empty()){
            context.colorHistory.pop();
            return;
        }

        if(args.size() > 1){
            context.colorHistory.pop();
        }

        if(std::string_view arg = args.front(); arg.starts_with('[')){
            arg.remove_prefix(1);

            if(arg.empty()){
                context.colorHistory.pop();
            }else{
                const auto color = Graphic::Color::valueOf(arg);
                context.colorHistory.push(color);
            }
        }
    };

    parser.modifiers["c"] = parser.modifiers["color"];

    parser.modifiers["off"] = [](const std::vector<std::string_view>& args, Context& context, const std::shared_ptr<Layout>& target){
        if(args.empty()){
            context.popOffset();
            return;
        }

        const auto offset = Func::string_cast_seq<float>(args.front(), 0, 2);

        switch(offset.size()){
            case 1: context.pushOffset({0, offset[0]}); break;
            case 2: context.pushOffset({offset[0], offset[1]}); break;
            default: context.popOffset();
        }
    };

    parser.modifiers["offs"] = [](const std::vector<std::string_view>& args, Context& context, const std::shared_ptr<Layout>& target){
        if(args.empty()){
            context.popOffset();
            return;
        }

        const auto offset = Func::string_cast_seq<float>(args.front(), 0, 2);

        switch(offset.size()){
            case 1: context.pushScaledOffset({0, offset[0]}); break;
            case 2: context.pushScaledOffset({offset[0], offset[1]}); break;
            default: context.popOffset();
        }
    };

    parser.modifiers["font"] = [](const std::vector<std::string_view>& args, Context& context, const std::shared_ptr<Layout>& target){
        if(args.empty()){
            context.fontHistory.pop();
            return;
        }

        Font::FontFaceID faceId{};
        if(const std::string_view arg = args.front(); arg.starts_with('[')){
            faceId = Func::string_cast<Font::FontFaceID>(arg.substr(1));
        }else{
            faceId = Font::namedFonts.at(arg, 0);
        }

        if(auto* font = Font::GlobalFontManager->getFontFace(faceId)){
            context.fontHistory.push(font);
        }
    };

    parser.modifiers["_"] = parser.modifiers["sub"] = [](const std::vector<std::string_view>& args, Context& context, const std::shared_ptr<Layout>& target){
        Func::beginSubscript(context, target);
    };

    parser.modifiers["^"] = parser.modifiers["sup"] = [](const std::vector<std::string_view>& args, Context& context, const std::shared_ptr<Layout>& target){
        Func::beginSuperscript(context, target);
    };

    parser.modifiers["\\"] = parser.modifiers["\\sup"] = parser.modifiers["\\sub"] = [](const std::vector<std::string_view>& args, Context& context, const std::shared_ptr<Layout>& target){
        Func::endScript(context, target);
    };
}

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

    loadParser();

    return fontManager;
}


// int main(){
//     std::map<std::string, int> map{};
//     map["123"];
//     std::vector<int> arr{};
//     auto rst = arr | std::views::filter([](auto i){return i & 1;});
//
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
    vulkanManager->registerResizeCallback([rendererUi](auto& e) {
        if(e.area())rendererUi->resize(static_cast<Geom::USize2>(e));
    }, [&rendererUi](auto e) {
        rendererUi->initRenderPass(e);
    });

    vulkanManager->uiImageViewProv = [rendererUi]{
        return std::make_pair(rendererUi->mergeFrameBuffer.at(3), rendererUi->mergeFrameBuffer.at(4));
    };

    auto* batch = new Graphic::Batch{vulkanManager->context, sizeof(Vulkan::Vertex_World)};
    vulkanManager->batchVertexData = batch->getBatchData();
	vulkanManager->initPipeline();

    auto gcm = Assets::PostProcess::Factory::gaussianFactory.generate(vulkanManager->swapChain.size2D(), {});


    File file{R"(D:\projects\vulkan_framework\properties\resource\assets\parse_test.txt)"};

    auto imageAtlas = loadTex();

    Font::FontManager fontManager = initFontManager(imageAtlas);

    std::shared_ptr<Font::TypeSettings::Layout> layout{new Font::TypeSettings::Layout};
    std::shared_ptr<Font::TypeSettings::Layout> fps{new Font::TypeSettings::Layout};
    layout->setAlign(Align::Pos::right);

    parser.requestParseInstantly(layout, file.readString());
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

        vulkanManager->updateBatchUniformBuffer(Vulkan::UniformBlock{mainCamera->getWorldToScreen(), 0.f});
        vulkanManager->updateCameraScale(mainCamera->getScale());

        constexpr auto baseColor = Graphic::Colors::WHITE;
        constexpr auto lightColor = Graphic::Colors::CLEAR.copy().appendLightColor(Graphic::Colors::WHITE);

        Geom::Vec2 off{ timer.getGlobalTime() * 5.f + 0, 0};
        Geom::Vec2 off2 = off.copy().add(50, 50);


        // rendererUi->pushScissor({100, 100, 500, 500});
        Font::TypeSettings::draw(rendererUi->batch, layout, {200, 200});

        sec += timer.getGlobalDelta();
        if(sec > 1.f){
            sec = 0;
            parser.requestParseInstantly(fps, std::to_string(fps_count));
            fps_count = 0;
        }
        fps_count++;

        rendererUi->batch.consumeAll();
        rendererUi->blit();

        Font::TypeSettings::draw(rendererUi->batch, fps, {300, 300});

        // rendererUi->pushScissor({200, 200, 1200, 1200}, false);
        Font::TypeSettings::draw(rendererUi->batch, layout, {100, 100});
        rendererUi->batch.consumeAll();

        rendererUi->blit();
        rendererUi->endBlit();

        vulkanManager->blitToScreen();

        rendererUi->clear();
    }

    vkDeviceWaitIdle(vulkanManager->context.device);


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

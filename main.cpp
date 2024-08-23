#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

// #include "src/application_head.h"

import std;
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

import Core.Vulkan.Shader.Compile;

import Assets.Graphic;
import Assets.Directories;

Core::Vulkan::Texture texturePester{};
Core::Vulkan::Texture texturePesterLight{};


int main(){
    using namespace Core;

	init();

    {
        const Vulkan::ShaderRuntimeCompiler compiler{};
        const Vulkan::ShaderCompilerWriter adaptor{compiler, Assets::Dir::shader_spv};

        File{Assets::Dir::shader_src}.forSubs([&](Core::File&& file){
            adaptor.compile(file);
        });
    }

	Assets::load(vulkanManager->context);
	vulkanManager->initVulkan();

	texturePester = Vulkan::Texture{vulkanManager->context.physicalDevice, vulkanManager->context.device};
	texturePester.loadPixmap(vulkanManager->obtainTransientCommand(), Assets::Dir::texture / R"(pester.png)");

	texturePesterLight = Vulkan::Texture{vulkanManager->context.physicalDevice, vulkanManager->context.device};
	texturePesterLight.loadPixmap(vulkanManager->obtainTransientCommand(), Assets::Dir::texture / R"(pester.light.png)");

    auto* batch = new Graphic::Batch{vulkanManager->context, sizeof(Vulkan::BatchVertex)};

    vulkanManager->batchVertexData = batch->getBatchData();

    batch->externalDrawCall = [](const Graphic::Batch& b, const bool isLast){
        vulkanManager->drawFrame(isLast, b.fence);
    };

    batch->descriptorChangedCallback = [](std::span<const VkImageView> data){
        vulkanManager->updateBatchDescriptorSet(data);
        vulkanManager->createBatchDrawCommands();
    };

    while(window && !window->shouldClose()) {
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

        {
            auto [imageIndex, dataPtr, captureLock] = batch->getDrawArgs(texturePester.getView());
            new(dataPtr) std::array{
                    Vulkan::BatchVertex{Geom::Vec2{0  , 0  }.add(off), 0.5f, {imageIndex}, baseColor, {0.0f, 1.0f}},
                    Vulkan::BatchVertex{Geom::Vec2{800, 0  }.add(off), 0.5f, {imageIndex}, baseColor, {1.0f, 1.0f}},
                    Vulkan::BatchVertex{Geom::Vec2{800, 800}.add(off), 0.5f, {imageIndex}, baseColor, {1.0f, 0.0f}},
                    Vulkan::BatchVertex{Geom::Vec2{0  , 800}.add(off), 0.5f, {imageIndex}, baseColor, {0.0f, 0.0f}},
                };
        }

        {
            auto [imageIndex, dataPtr, captureLock] = batch->getDrawArgs(texturePesterLight.getView());
            new(dataPtr) std::array{
                    Vulkan::BatchVertex{Geom::Vec2{0  , 0  }.add(off), 0.5f, {imageIndex}, lightColor, {0.0f, 1.0f}},
                    Vulkan::BatchVertex{Geom::Vec2{800, 0  }.add(off), 0.5f, {imageIndex}, lightColor, {1.0f, 1.0f}},
                    Vulkan::BatchVertex{Geom::Vec2{800, 800}.add(off), 0.5f, {imageIndex}, lightColor, {1.0f, 0.0f}},
                    Vulkan::BatchVertex{Geom::Vec2{0  , 800}.add(off), 0.5f, {imageIndex}, lightColor, {0.0f, 0.0f}},
                };
        }

        {
            auto [imageIndex, dataPtr, captureLock] = batch->getDrawArgs(texturePester.getView());
            new(dataPtr) std::array{
                    Vulkan::BatchVertex{Geom::Vec2{0  , 0  }.add(off2), 0.7f, {imageIndex}, baseColor, {0.0f, 1.0f}},
                    Vulkan::BatchVertex{Geom::Vec2{800, 0  }.add(off2), 0.7f, {imageIndex}, baseColor, {1.0f, 1.0f}},
                    Vulkan::BatchVertex{Geom::Vec2{800, 800}.add(off2), 0.7f, {imageIndex}, baseColor, {1.0f, 0.0f}},
                    Vulkan::BatchVertex{Geom::Vec2{0  , 800}.add(off2), 0.7f, {imageIndex}, baseColor, {0.0f, 0.0f}},
                };
        }

        {
            auto [imageIndex, dataPtr, captureLock] = batch->getDrawArgs(texturePesterLight.getView());
            new(dataPtr) std::array{
                    Vulkan::BatchVertex{Geom::Vec2{0  , 0  }.add(off2), 0.7f, {imageIndex}, lightColor, {0.0f, 1.0f}},
                    Vulkan::BatchVertex{Geom::Vec2{800, 0  }.add(off2), 0.7f, {imageIndex}, lightColor, {1.0f, 1.0f}},
                    Vulkan::BatchVertex{Geom::Vec2{800, 800}.add(off2), 0.7f, {imageIndex}, lightColor, {1.0f, 0.0f}},
                    Vulkan::BatchVertex{Geom::Vec2{0  , 800}.add(off2), 0.7f, {imageIndex}, lightColor, {0.0f, 0.0f}},
                };
        }

        batch->consumeAll();

        vulkanManager->blitToScreen();
    }

    vkDeviceWaitIdle(vulkanManager->context.device);

    delete batch;
    texturePester = {};
	texturePesterLight = {};

	Assets::dispose();

	terminate();

	return 0;//just for main func test swap...
}

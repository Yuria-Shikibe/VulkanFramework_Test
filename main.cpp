#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

import std;
import Core.Window;
import Core.Vulkan.Manager;
import Core.Vulkan.Instance;
import Core.Vulkan.Vertex;
import Core.Vulkan.Uniform;
import Core.Vulkan.Image;
import Core.Vulkan.Texture;
import Core.Vulkan.Attachment;

import Core.InitAndTerminate;

import OS.File;
import Graphic.Color;
import Graphic.Batch;
import Graphic.Pixmap;

import Geom.Matrix4D;

import Core.Vulkan.Shader.Compile;

import Assets.Graphic;

Core::Vulkan::Texture texturePester{};
Core::Vulkan::Texture texturePesterLight{};


int main(){
    using namespace Core;



    {
        Vulkan::ShaderRuntimeCompiler compiler{};
        Vulkan::ShaderCompilerWriter adaptor{compiler, Vulkan::TargetCompilerPath};

        OS::File{Vulkan::DefaultSrcPath}.forSubs([&](OS::File&& file){
            adaptor.compile(file);
        });
    }

    Assets::Shader::builtinShaderDir = Vulkan::TargetCompilerPath;

	init();
	Assets::load(vulkanManager->context);
	vulkanManager->initVulkan();

	texturePester = Vulkan::Texture{vulkanManager->context.physicalDevice, vulkanManager->context.device};
	texturePester.loadPixmap(vulkanManager->obtainTransientCommand(), R"(D:\projects\vulkan_framework\properties\texture\pester.png)");

	texturePesterLight = Vulkan::Texture{vulkanManager->context.physicalDevice, vulkanManager->context.device};
	texturePesterLight.loadPixmap(vulkanManager->obtainTransientCommand(), R"(D:\projects\vulkan_framework\properties\texture\pester.light.png)");

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
        window->pollEvents();

        float t = glfwGetTime();

        Geom::Matrix3D matrix3D{};
        matrix3D.setOrthogonal(vulkanManager->swapChain.getTargetWindow()->getSize().as<float>());
        matrix3D.rotate(30);

        vulkanManager->updateUniformBuffer(Vulkan::UniformBlock{matrix3D, 0.f});

        constexpr auto baseColor = Graphic::Colors::WHITE;
        constexpr auto lightColor = Graphic::Colors::CLEAR.copy().appendLightColor(Graphic::Colors::WHITE);

        Geom::Vec2 off{200 + t * 5.f + 300, -100};
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
	GLFW::terminate();

	return 0;//just for main func test swap...
}

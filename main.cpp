#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

import std;
import Core.Window;
import Core.Vulkan.Manager;
import Core.Vulkan.Shader.Compile;
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

import Assets.Graphic;

std::array<Core::Vulkan::Texture, 3> textures{};

int main(){
	using namespace Core;
	Assets::Shader::builtinShaderDir = Vulkan::TargetCompilerPath;

	OS::File{Vulkan::DefaultSrcPath}.forSubs([](OS::File&& file){
		Vulkan::ShaderCompileTask{
				Vulkan::DefaultCompilerPath, file, Vulkan::TargetCompilerPath
			}.compile();
	});

	Core::init();

	Assets::load(vulkanManager->context);

	vulkanManager->initVulkan();

	for(const auto& [i, texture] : textures | std::views::enumerate){
		std::string p = std::format(R"(D:\projects\vulkan_framework\properties\texture\t{}.png)", i);
		texture = Core::Vulkan::Texture{
				vulkanManager->context.physicalDevice, vulkanManager->context.device,
				OS::File{p},
				vulkanManager->obtainTransientCommand()
			};
	}

	{
		Graphic::Batch<Vulkan::BatchVertex> batch{};

		batch.init(&vulkanManager->context, Assets::Sampler::textureDefaultSampler.get());

		vulkanManager->batchVertexData = batch.getBatchData();

		batch.externalDrawCall = [](const decltype(batch)& b, const bool isLast){
			vulkanManager->drawFrame(isLast, b.fence);
		};

		batch.descriptorChangedCallback = [](auto data){
			vulkanManager->updateDescriptorSet(data);
			if(vulkanManager->swapChain.isResized()){
				vulkanManager->swapChain.recreate();
			}else{
				vulkanManager->createDrawCommands();
			}
		};

		batch.updateDescriptorSets();

		while(window && !window->shouldClose()) {
			window->pollEvents();

			float t = glfwGetTime();
			// vulkanManager->drawBegin();

			Geom::Matrix3D matrix3D{};
			matrix3D.setOrthogonal(vulkanManager->swapChain.getTargetWindow()->getSize().as<float>());

			vulkanManager->updateUniformBuffer(Vulkan::UniformBlock{matrix3D, 0.f});

			for(std::uint32_t x = 0; x < 10; ++x){
				for(const auto& [i, texture] : textures | std::views::enumerate){
					Geom::Vec2 off{x * 300.f + t * 10.f, 500.f * i};
					auto [imageIndex, dataPtr, captureLock] = batch.getDrawArgs(texture.getView());
					new(dataPtr) std::array{
						Vulkan::BatchVertex{Geom::Vec2{0  , 0  }.add(off), 0.9f - i / 10.f, imageIndex, Graphic::Colors::WHITE, {0.0f, 1.0f}},
						Vulkan::BatchVertex{Geom::Vec2{500, 0  }.add(off), 0.9f - i / 10.f, imageIndex, Graphic::Colors::WHITE, {1.0f, 1.0f}},
						Vulkan::BatchVertex{Geom::Vec2{500, 500}.add(off), 0.9f - i / 10.f, imageIndex, Graphic::Colors::CLEAR, {1.0f, 0.0f}},
						Vulkan::BatchVertex{Geom::Vec2{0  , 500}.add(off), 0.9f - i / 10.f, imageIndex, Graphic::Colors::CLEAR, {0.0f, 0.0f}},
					};
				}
			}



			batch.consumeAll();

			vulkanManager->blitToScreen();
		}

		vkDeviceWaitIdle(vulkanManager->context.device);
	}

	textures = {};

	Assets::dispose();

	Core::terminate();
	GLFW::terminate();

	return 0;//just for main func test swap...
}

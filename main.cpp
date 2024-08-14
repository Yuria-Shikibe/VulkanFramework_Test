#include <vulkan/vulkan.h>

import std;
import Core.Window;
import Core.Vulkan.Manager;
import Core.Vulkan.Shader.Compile;
import Core.Vulkan.Instance;
import Core.Vulkan.Vertex;
import Core.Vulkan.Uniform;
import Core.Vulkan.Image;

import Core.InitAndTerminate;

import OS.File;
import Graphic.Color;
import Graphic.Batch;

import Assets.Graphic;

std::array<Core::Vulkan::Texture, 8> textures{};

int main(){
	using namespace Core;
	Assets::Shader::builtinShaderDir = Vulkan::TargetCompilerPath;

	Vulkan::ShaderCompileTask{
		Vulkan::DefaultCompilerPath,
		Vulkan::DefaultSrcPath / "test.frag",
		Vulkan::TargetCompilerPath
	}.compile();

	Vulkan::ShaderCompileTask{
		Vulkan::DefaultCompilerPath,
		Vulkan::DefaultSrcPath / "test.vert",
		Vulkan::TargetCompilerPath
	}.compile();

	Core::init();

	Assets::load(vulkanManager->context.device);

	for(const auto& [i, texture] : textures | std::views::enumerate){
		std::string p = std::format(R"(D:\projects\vulkan_framework\properties\texture\test-{}.png)", i);
		texture = Core::Vulkan::Texture{
				vulkanManager->context.physicalDevice, vulkanManager->context.device,
				OS::File{p},
				vulkanManager->transientCommandPool.obtainTransient(vulkanManager->context.device.getGraphicsQueue())
			};
	}

	{
		Graphic::Batch<Vulkan::Vertex> batch{};

		batch.init(&vulkanManager->context, Assets::Sampler::textureDefaultSampler.get());
		batch.initUniformBuffer<Vulkan::UniformBlock>();

		{
			auto& info = vulkanManager->usedPipelineResources;

			info.indexBuffer = batch.buffer_index;
			info.indexType = batch.buffer_index.indexType;
			info.vertexBuffer = batch.buffer_vertex;
			info.indirectBuffer = batch.buffer_indirect.getTargetBuffer();

			info.descriptorSetLayout = batch.descriptorSetLayout;
			info.descriptorSet = batch.descriptorSet;
		}

		batch.commandRecordCallback = []{
			vulkanManager->createCommandBuffers();
		};

		batch.externalDrawCall = [](const decltype(batch)& b, const bool isLast){
			vulkanManager->drawFrame(b.semaphore_copyDone, b.semaphore_drawDone, isLast);
		};

		batch.updateDescriptorSetsPure();

		vulkanManager->postInitVulkan();



		while(window && !window->shouldClose()) {
			window->pollEvents();

			vulkanManager->drawBegin();

			Geom::Matrix3D matrix3D{};
			matrix3D.setOrthogonal(vulkanManager->swapChain.getTargetWindow()->getSize().as<float>());
			batch.updateUniformBuffer(Vulkan::UniformBlock{matrix3D, 0.f});

			for(const auto& [i, texture] : textures | std::views::enumerate){
				auto [imageIndex, dataPtr, captureLock] = batch.getDrawArgs(texture.getView());
				new(dataPtr) std::array{
					Vulkan::Vertex{Geom::Vec2{0 , 0 }.addScaled({50, 50}, i), 0, imageIndex, Graphic::Colors::WHITE, {0.0f, 1.0f}},
					Vulkan::Vertex{Geom::Vec2{50, 0 }.addScaled({50, 50}, i), 0, imageIndex, Graphic::Colors::WHITE, {1.0f, 1.0f}},
					Vulkan::Vertex{Geom::Vec2{50, 50}.addScaled({50, 50}, i), 0, imageIndex, Graphic::Colors::WHITE, {1.0f, 0.0f}},
					Vulkan::Vertex{Geom::Vec2{0 , 50}.addScaled({50, 50}, i), 0, imageIndex, Graphic::Colors::WHITE, {0.0f, 0.0f}},
				};
			}


			batch.consumeAll();

			vulkanManager->drawEnd();

			//
			// if (auto [p, failed] = vulkanManager->dyVertexBuffer.acquireSegment(); !failed){
			// 	new(p) std::array{
			// 		Vulkan::Vertex{Geom::Vec2{50 , 50 }.add(400, 400), 0, 1, Graphic::Colors::WHITE, {0.0f, 1.0f}},
			// 		Vulkan::Vertex{Geom::Vec2{150, 50 }.add(400, 400), 0, 1, Graphic::Colors::WHITE, {1.0f, 1.0f}},
			// 		Vulkan::Vertex{Geom::Vec2{150, 150}.add(400, 400), 0, 1, Graphic::Colors::WHITE, {1.0f, 0.0f}},
			// 		Vulkan::Vertex{Geom::Vec2{50 , 150}.add(400, 400), 0, 1, Graphic::Colors::WHITE, {0.0f, 0.0f}},
			// 	};
			// }
		}

		vkDeviceWaitIdle(vulkanManager->context.device);
	}

	for(const auto& [i, texture] : textures | std::views::enumerate){
		texture = Vulkan::Texture{};
	}

	Assets::dispose();

	terminate();
	GLFW::terminate();
}

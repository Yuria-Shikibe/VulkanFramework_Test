#include <vulkan/vulkan.h>

import std;
import Core.Window;
import Core.Vulkan.Manager;
import Core.Vulkan.Shader.Compile;
import Core.Vulkan.Instance;
import Core.Vulkan.Vertex;

import Core.InitAndTerminate;

import OS.File;
import Graphic.Color;

import Assets.Graphic;

int main(){
	Assets::Shader::builtinShaderDir = Core::Vulkan::TargetCompilerPath;

	Core::Vulkan::ShaderCompileTask{
		Core::Vulkan::DefaultCompilerPath,
		Core::Vulkan::DefaultSrcPath / "test.frag",
		Core::Vulkan::TargetCompilerPath
	}.compile();

	Core::Vulkan::ShaderCompileTask{
		Core::Vulkan::DefaultCompilerPath,
		Core::Vulkan::DefaultSrcPath / "test.vert",
		Core::Vulkan::TargetCompilerPath
	}.compile();

	Core::init();
	Assets::Shader::load(Core::vulkanManager->context.device);
	Core::vulkanManager->postInitVulkan();

	while(Core::window && !Core::window->shouldClose()) {
		Core::window->pollEvents();
		if (auto [p, failed] = Core::vulkanManager->dyVertexBuffer.acquireSegment(); !failed){
			new(p) std::array{
					Core::Vulkan::Vertex{{50 , 50 }, 0.f, {1.0f, 0.0f, 0.0f, 1.f}, {0.0f, 1.0f}},
					Core::Vulkan::Vertex{{250, 50 }, 0.f, {0.0f, 1.0f, 0.0f, 1.f}, {1.0f, 1.0f}},
					Core::Vulkan::Vertex{{250, 250}, 0.f, {0.0f, 0.0f, 1.0f, .17f}, {1.0f, 0.0f}},
					Core::Vulkan::Vertex{{50 , 250}, 0.f, {1.0f, 1.0f, 1.0f, 1.f }, {0.0f, 0.0f}},
				};
		}

		if (auto [p, failed] = Core::vulkanManager->dyVertexBuffer.acquireSegment(); !failed){
			new(p) std::array{
					Core::Vulkan::Vertex{Geom::Vec2{50 , 50 }.add(400, 400), 0.f, Graphic::Colors::WHITE, {0.0f, 1.0f}},
					Core::Vulkan::Vertex{Geom::Vec2{150, 50 }.add(400, 400), 0.f, Graphic::Colors::WHITE, {1.0f, 1.0f}},
					Core::Vulkan::Vertex{Geom::Vec2{150, 150}.add(400, 400), 0.f, Graphic::Colors::WHITE, {1.0f, 0.0f}},
					Core::Vulkan::Vertex{Geom::Vec2{50 , 150}.add(400, 400), 0.f, Graphic::Colors::WHITE, {0.0f, 0.0f}},
				};
		}

		Core::vulkanManager->drawFrame();
	}

	vkDeviceWaitIdle(Core::vulkanManager->context.device);

	Assets::Shader::dispose();

	Core::terminate();
	Core::GLFW::terminate();
}

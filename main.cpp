#include <vulkan/vulkan.h>

import std;
import Core.Window;
import Core.Vulkan.Manager;
import Core.Vulkan.Shader.Compile;
import Core.Vulkan.Instance;

import Core;

import OS.File;

int main(){

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
	// Core::GLFW::init();
	// Core::Vulkan::VkCore vkCore{};

	// vkCore.init();

	while(!Core::window->shouldClose()) {
		Core::window->pollEvents();
		Core::vulkanManager->drawFrame();
	}

	vkDeviceWaitIdle(Core::vulkanManager->device);

	Core::terminate();
	Core::GLFW::terminate();
	Core::GLFW::terminate();
	Core::GLFW::terminate();
	Core::GLFW::terminate();
	Core::GLFW::terminate();
}

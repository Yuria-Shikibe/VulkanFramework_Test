//
// Created by Matrix on 2024/8/3.
//

export module Core.InitAndTerminate;

export import Core.Global;
export import Core.Vulkan.Manager;
export import Core.Vulkan.Memory;
export import Core.Window;

export namespace Core{
	void init(){
		GLFW::init();

		window = new Window{};
		vulkanManager = new Vulkan::VulkanManager;
		vulkanManager->preInitVulkan(window);
	}

	void terminate(){
		delete vulkanManager;
		delete window;

		GLFW::terminate();
	}
}

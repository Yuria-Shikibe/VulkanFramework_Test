//
// Created by Matrix on 2024/8/3.
//

export module Core;

export import Core.Global;
export import Core.Vulkan.Manager;
export import Core.Window;

export namespace Core{
	void init(){
		GLFW::init();

		window = new Window{};
		vulkanManager = new Vulkan::VulkanManager{window->handle};
	}

	void terminate(){
		delete vulkanManager;
		delete window;
	}
}

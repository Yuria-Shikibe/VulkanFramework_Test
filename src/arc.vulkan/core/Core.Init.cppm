module;

#include <GLFW/glfw3.h>

export module Core.InitAndTerminate;

import Core.Global;
import Core.Vulkan.Manager;
import Core.Vulkan.Memory;
import Core.Window;
import Graphic.Camera2D;
import Core.Input;

static double getTime(){
    return (glfwGetTime());
}
static double getDelta(const double last){
    return (glfwGetTime()) - last;
}

export namespace Core{
    void initFileSystem();

    void bindFocus();

    void bindCtrlCommands();

    void init(){
        GLFW::init();

        initFileSystem();

        input = new Ctrl::Input;
        mainCamera = new Graphic::Camera2D;
        window = new Window;
        vulkanManager = new Vulkan::VulkanManager;
        vulkanManager->initContext(window);

        timer = ApplicationTimer<float>{getDelta, getTime, glfwSetTime};

        window->registerResizeCallback("mainCameraResizeEvent", [](const Window::ResizeEvent& e) {
            mainCamera->resize(e.size.x, e.size.y);
        });

	    bindFocus();
        bindCtrlCommands();
    }

	void terminate(){
	    delete input;
		delete mainCamera;
		delete vulkanManager;
		delete window;

		GLFW::terminate();
	}
}

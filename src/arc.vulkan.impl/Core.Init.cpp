module;

#include <GLFW/glfw3.h>



module Core.InitAndTerminate;

import Core.Vulkan.Manager;
import Core.Vulkan.Memory;
import Core.Window;
import Graphic.Renderer.World;
import Graphic.Renderer.UI;
import Graphic.Camera2D;
import Core.Input;

import Assets.Ctrl;
import Assets.Graphic;
import Assets.Directories;
import Core.Global.Focus;

import Core.FilePlain;

import Core.Vulkan.EXT;
import Core.Global;

import std;

static double getTime(){
    return (glfwGetTime());
}

static void resetTime(const double t){
    glfwSetTime(t);
}

static double getDelta(const double last){
    return (glfwGetTime()) - last;
}

void Core::bindFocus() {
    Focus::camera = Ctrl::CameraFocus{mainCamera, mainCamera};
}

void Core::initFileSystem() {
#if defined(ASSETS_DIR)
    const File dir{ASSETS_DIR};
#else
    const File dir{std::filesystem::current_path()};
#endif

    std::cout << "[Assets] Targeted Resource Root:" << dir.absolutePath() << std::endl;
    Assets::Dir::files = FilePlane{dir};

    Assets::loadDir();
}

void Core::bindCtrlCommands() {
    Assets::Ctrl::load();
}

void Core::loadEXT(){
    Core::Vulkan::EXT::load(vulkanManager->context.instance);
}

void Core::init(){
    GLFW::init();

    initFileSystem();

    input = new Ctrl::Input;
    mainCamera = new Graphic::Camera2D;
    window = new Window;
    vulkanManager = new Vulkan::VulkanManager;
    vulkanManager->initContext(window);


    loadEXT();

    timer = ApplicationTimer<float>{getDelta, getTime, resetTime};

    window->registerResizeCallback("mainCameraResizeEvent", [](const Window::ResizeEvent& e) {
        mainCamera->resize(e.size.x, e.size.y);
    });

    bindFocus();
    bindCtrlCommands();
}

void Core::postInit(){
    rendererUI = new Graphic::RendererUI{vulkanManager->context};
    rendererWorld = new Graphic::RendererWorld{vulkanManager->context};

    vulkanManager->registerResizeCallback(
        [](auto& e){
            if(e.area()){
                rendererUI->resize(static_cast<Geom::USize2>(e));
                vulkanManager->uiPort = rendererUI->getPort();
            }
        }, [](auto e){
            rendererUI->init(e);
            vulkanManager->uiPort = rendererUI->getPort();
        });


    vulkanManager->registerResizeCallback(
    [](auto& e){
        if(e.area()){
            rendererWorld->resize(static_cast<Geom::USize2>(e));
            vulkanManager->worldPort = rendererWorld->getPort();
        }
    }, [](auto e){
        rendererWorld->init(e);
        vulkanManager->worldPort = rendererWorld->getPort();
    });

    vulkanManager->initPipeline();
}

void Core::terminate(){
    delete rendererWorld;
    delete rendererUI;

    delete input;
    delete mainCamera;
    delete vulkanManager;
    delete window;

    GLFW::terminate();
}

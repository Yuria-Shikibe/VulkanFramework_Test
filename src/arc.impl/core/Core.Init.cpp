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
import Core.Global.Graphic;
import Core.Global.UI;

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

void Core::Global::initFileSystem() {
#if defined(ASSETS_DIR)
    const File dir{ASSETS_DIR};
#else
    const File dir{std::filesystem::current_path()};
#endif

    std::cout << "[Assets] Targeted Resource Root:" << dir.absolutePath() << std::endl;
    Assets::Dir::files = FilePlane{dir};

    Assets::loadDir();
}

void Core::Global::initCtrl() {
    Focus::camera = Ctrl::CameraFocus{mainCamera, mainCamera};
    Assets::Ctrl::load();
}

void Core::Global::loadEXT(){
    Core::Vulkan::EXT::load(vulkanManager->context.instance);
}

void Core::Global::init(){
    GLFW::init();

    initFileSystem();

    input = new Ctrl::Input;

    //Window Init
    window = new Window;


    //Vulkan Context Init
    vulkanManager = new Vulkan::VulkanManager;
    vulkanManager->initContext(window);
    loadEXT();


    //Camera Init
    mainCamera = new Graphic::Camera2D;
    window->registerResizeCallback("mainCameraResizeEvent", [](const Window::ResizeEvent& e) {
        mainCamera->resize(e.size.x, e.size.y);
    });


    //Timer Init
    timer = ApplicationTimer<float>{getDelta, getTime, resetTime};


    //Ctrl Init
    initCtrl();
}

void Core::Global::initUI(){
    UI::init(*vulkanManager);
}

void Core::Global::postInit(){
    initUI();

    rendererWorld = new Graphic::RendererWorld{vulkanManager->context};
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

void Core::Global::terminate(){
    UI::terminate();

    delete rendererWorld;

    delete input;
    delete mainCamera;
    delete vulkanManager;
    delete window;

    GLFW::terminate();
}

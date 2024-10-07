export module Core.Global;

export import Core.ApplicationTimer;

export namespace Graphic {
    class Camera2D;
}


export namespace Core {
    namespace Ctrl {
        class Input;
    }


    namespace Vulkan {
        class VulkanManager;
    }

    struct Window;
}


export namespace Core::Global{
    //TODO move this into main loop manager
    ApplicationTimer<float> timer{};
    Ctrl::Input* input{};
    Window* window{};
    Graphic::Camera2D* mainCamera{};
	Vulkan::VulkanManager* vulkanManager{};
}

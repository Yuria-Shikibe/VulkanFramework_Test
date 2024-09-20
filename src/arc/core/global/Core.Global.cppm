export module Core.Global;

export import Core.ApplicationTimer;

export namespace Graphic {
    struct RendererUI;
    struct RendererWorld;
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
    ApplicationTimer<float> timer{};
    Ctrl::Input* input{};
    Window* window{};
    Graphic::Camera2D* mainCamera{};
	Vulkan::VulkanManager* vulkanManager{};
}

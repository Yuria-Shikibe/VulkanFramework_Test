//
// Created by Matrix on 2024/8/3.
//

export module Core.Global;

export import Core.Vulkan.Manager;
export import Core.Window;

export namespace Core{
	Window* window{};
	Vulkan::VulkanManager* vulkanManager{};
}

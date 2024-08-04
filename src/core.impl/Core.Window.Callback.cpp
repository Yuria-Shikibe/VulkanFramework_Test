module Core.Window.Callback;

import Core.Global;
import std;

void Core::GLFW::framebufferResizeCallback(GLFWwindow* window, int width, int height){
	if constexpr (NullptrCheck){
		if(!vulkanManager)throw std::runtime_error("Null Vulkan Manager");
	}

	vulkanManager->framebufferResized = true;
}

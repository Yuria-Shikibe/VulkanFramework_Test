module;

#include <GLFW/glfw3.h>

export module Core.Window.Callback;

namespace Core::GLFW{
	constexpr bool NullptrCheck{DEBUG_CHECK};

	void framebufferResizeCallback(GLFWwindow* window, int width, int height);

	export void setCallBack(GLFWwindow* window){
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}
}

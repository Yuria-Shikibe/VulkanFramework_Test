module;

#include <GLFW/glfw3.h>

module Core.Window.Callback;

import Core.Window;
import Core.Global;
import std;

void Core::GLFW::framebufferResizeCallback(GLFWwindow* window, int width, int height){
	const auto app = static_cast<Window*>(glfwGetWindowUserPointer(window));

	app->resize(width, height);
}

void Core::GLFW::setCallBack(GLFWwindow* window, void* user){
	glfwSetWindowUserPointer(window, user);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

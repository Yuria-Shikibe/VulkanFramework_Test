module;

#include <GLFW/glfw3.h>

module Core.Window.Callback;

import Core.Window;
import Core.Input;
import Core.Global;
import std;


void Core::GLFW::framebufferResizeCallback(GLFWwindow* window, const int width, const int height){
	const auto app = static_cast<Window*>(glfwGetWindowUserPointer(window));

	app->resize(width, height);
}

void Core::GLFW::mouseBottomCallBack(GLFWwindow *window, const int button, const int action, const int mods) {
    input->informMouseAction(button, action, mods);
}

void Core::GLFW::cursorPosCallback(GLFWwindow *window, const double xPos, const double yPos) {
    const auto app = static_cast<Window*>(glfwGetWindowUserPointer(window));

    input->cursorMoveInform(static_cast<float>(xPos), static_cast<float>(app->getSize().y - yPos));
}

void Core::GLFW::cursorEnteredCallback(GLFWwindow *window, const int entered) {
    input->setInbound(entered);
}

void Core::GLFW::scrollCallback(GLFWwindow *window, const double xOffset, const double yOffset) {
    input->setScrollOffset(static_cast<float>(xOffset), static_cast<float>(yOffset));
}

void Core::GLFW::keyCallback(GLFWwindow *window, const int key, const int scanCode, const int action, const int mods) {
    if(key >= 0 && key < GLFW_KEY_LAST)input->informKeyAction(key, scanCode, action, mods);
}

void Core::GLFW::setCallBack(GLFWwindow* window, void* user){
	glfwSetWindowUserPointer(window, user);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetMouseButtonCallback(window, mouseBottomCallBack);
	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetCursorEnterCallback(window, cursorEnteredCallback);
	glfwSetKeyCallback(window, keyCallback);
}

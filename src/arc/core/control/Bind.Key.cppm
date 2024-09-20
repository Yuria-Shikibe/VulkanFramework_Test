module;

#include <GLFW/glfw3.h>

export module Core.Ctrl.Bind:Key;
import :InputBind;

import std;

export namespace Core::Ctrl{
	struct KeyBind : InputBind{
		using InputBind::InputBind;

		bool activated(GLFWwindow* window) const {
			return glfwGetKey(window, key) == expectedAct;
		}
	};
}

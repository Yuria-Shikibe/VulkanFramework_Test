module;

#include <GLFW/glfw3.h>

export module Core.Ctrl.Bind:Mouse;
import :InputBind;

import std;

export namespace Core::Ctrl{
	struct [[deprecated]] MouseBind : InputBind{
		using InputBind::InputBind;

		bool queryActivated(GLFWwindow* window) const noexcept{
			return glfwGetMouseButton(window, key) == act;
		}
	};
}



module;

#define GLFW_INCLUDE_VULKAN
#include <../include/GLFW/glfw3.h>

export module Core.Window;
import std;
import Core.Window.Callback;

export namespace Core{
	constexpr std::uint32_t WIDTH = 800;
	constexpr std::uint32_t HEIGHT = 600;

	namespace GLFW{
		void init(){
			glfwInit();
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		}
	}

	struct Window{
		GLFWwindow* handle{};

		[[nodiscard]] Window(){
			handle = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

			Core::GLFW::setCallBack(handle);
		}

		~Window(){
			glfwDestroyWindow(handle);
			glfwTerminate();
		}

		[[nodiscard]] bool shouldClose() const{
			return glfwWindowShouldClose(handle);
		}

		void pollEvents() const{
			glfwPollEvents();
		}
	};
}
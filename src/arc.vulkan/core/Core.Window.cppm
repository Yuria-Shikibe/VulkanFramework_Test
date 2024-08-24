module;

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

export module Core.Window;
import std;
import Core.Window.Callback;
import Geom.Vector2D;
import ext.Event;

export namespace Core{
	constexpr std::uint32_t WIDTH = 800;
	constexpr std::uint32_t HEIGHT = 600;

	namespace GLFW{
		void init(){
			glfwInit();
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		}

		void terminate(){
			glfwTerminate();
		}
	}

	struct Window{
		struct ResizeEvent final : ext::EventType{
			Geom::USize2 size{};

			[[nodiscard]] explicit ResizeEvent(const Geom::USize2& size)
				: size{size}{}
		};

		ext::NamedEventManager eventManager{
			ext::typeIndexOf<ResizeEvent>()
		};

		[[nodiscard]] Window(){
			handle = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

			Core::GLFW::setCallBack(handle, this);

			glfwGetWindowSize(handle, &size.x, &size.y);
		}

		~Window(){
			if(handle){
				glfwDestroyWindow(handle);
			}
		}

		[[nodiscard]] bool iconified() const{
			return glfwGetWindowAttrib(handle, GLFW_ICONIFIED) || size.area() == 0;
		}

		[[nodiscard]] bool shouldClose() const{
			return glfwWindowShouldClose(handle);
		}
	    template <std::regular_invocable<Geom::USize2> InitFunc>
	    void registerResizeCallback(const std::string_view name, std::function<void(const ResizeEvent&)>&& callback, InitFunc initFunc) {
		    initFunc(size.as<std::uint32_t>());
		    eventManager.on<ResizeEvent>(name, std::move(callback));
		}

	    void registerResizeCallback(const std::string_view name, std::function<void(const ResizeEvent&)>&& callback) {
		    callback(ResizeEvent{size.as<std::uint32_t>()});
		    eventManager.on<ResizeEvent>(name, std::move(callback));
		}

		void pollEvents() const{
			glfwPollEvents();
		}

		void waitEvent() const{
			glfwWaitEvents();
		}

		[[nodiscard]] VkSurfaceKHR createSurface(VkInstance instance) const{
			VkSurfaceKHR surface{};
			if(glfwCreateWindowSurface(instance, handle, nullptr, &surface) != VK_SUCCESS){
				throw std::runtime_error("Failed to create window surface!");
			}

			return surface;
		}

		[[nodiscard]] GLFWwindow* getHandle() const{ return handle; }

		[[nodiscard]] Geom::USize2 getSize() const{ return size.as<std::uint32_t>(); }

		void resize(const int w, const int h){
			if(w == size.x && h == size.y)return;
			size.set(w, h);
			eventManager.fire(ResizeEvent{size.as<std::uint32_t>()});
		}

		Window(const Window& other);

		Window(Window&& other) noexcept
			: eventManager{std::move(other.eventManager)},
			  handle{other.handle},
			  size{std::move(other.size)}{
			other.handle = nullptr;
		}

		Window& operator=(const Window& other);

		Window& operator=(Window&& other) noexcept{
			if(this == &other) return *this;
			eventManager = std::move(other.eventManager);
			handle = other.handle;
			size = std::move(other.size);
			other.handle = nullptr;
			return *this;
		}

	private:
		GLFWwindow* handle{};
		Geom::Vector2D<int> size{};
	};
}
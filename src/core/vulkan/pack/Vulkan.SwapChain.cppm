module;

#include <vulkan/vulkan.h>


export module Core.Vulkan.SwapChain;

import std;
import Core.Window;

export namespace Core::Vulkan{
	class SwapChain{
	public:
		static constexpr std::string_view SwapChainName{"SwapChain"};

	private:
		Window* targetWindow{};
		VkInstance instance{};
		VkSurfaceKHR surface{};

	public:
		void attachWindow(Window* window){
			if(targetWindow)throw std::runtime_error("Target window already set!");

			targetWindow = window;

			targetWindow->eventManager.on<Window::ResizeEvent>(SwapChainName, [this](const auto& event){
				this->resize(event.size.x, event.size.y);
			});
		}

		void detachWindow(){
			if(!targetWindow)return;

			(void)targetWindow->eventManager.erase<Window::ResizeEvent>(SwapChainName);

			targetWindow = nullptr;

			destroySurface();
		}

		void createSurface(VkInstance instance){
			if(surface)throw std::runtime_error("Surface window already set!");

			if(!targetWindow)throw std::runtime_error("Missing Window!");

			surface = targetWindow->createSurface(instance);
			this->instance = instance;
		}

		void destroySurface(){
			if(surface && instance)vkDestroySurfaceKHR(instance, surface, nullptr);

			instance = nullptr;
			surface = nullptr;
		}

		~SwapChain(){
			detachWindow();
			destroySurface();
		}

		[[nodiscard]] Window* getTargetWindow() const{ return targetWindow; }

		[[nodiscard]] VkInstance getInstance() const{ return instance; }

		[[nodiscard]] VkSurfaceKHR getSurface() const{ return surface; }

		void resize(int w, int h){

		}
	};
}
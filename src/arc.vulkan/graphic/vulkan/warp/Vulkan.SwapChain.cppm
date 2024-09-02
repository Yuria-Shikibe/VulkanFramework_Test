module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.SwapChain;

import Core.Window;
import Core.Vulkan.Util.Invoker;
import Core.Vulkan.SwapChainInfo;
import Core.Vulkan.PhysicalDevice;

import Core.Vulkan.Image;

import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Buffer.FrameBuffer;

import std;

export namespace Core::Vulkan{
	constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT = 2;

	class SwapChain{
	public:
		static constexpr std::string_view SwapChainName{"SwapChain"};

	private:
		Window* targetWindow{};
		VkInstance instance{};

		VkPhysicalDevice physicalDevice{};
		VkDevice device{};

		VkSurfaceKHR surface{};

		VkSwapchainKHR swapChain{};

		mutable VkSwapchainKHR oldSwapChain{};

		VkFormat swapChainImageFormat{};

		struct SwapChainFrameData{
			VkImage image{};
			ImageView imageView{};
			FramebufferLocal framebuffer{};
			CommandBuffer commandFlush{};
		};

		std::vector<SwapChainFrameData> swapChainImages{};

		mutable bool resized{};

		void resize(int w, int h){
			resized = true;
		}

		std::uint32_t currentInFlightFrame{};
		std::uint32_t currentRenderingFrame{};

	public:
		std::function<void(SwapChain&)>  recreateCallback{};
		VkQueue presentQueue{};

		[[nodiscard]] SwapChain() = default;

		void attachWindow(Window* window){
			if(targetWindow) throw std::runtime_error("Target window already set!");

			targetWindow = window;

			targetWindow->eventManager.on<Window::ResizeEvent>(SwapChainName, [this](const Window::ResizeEvent& event){
				this->resize(event.size.x, event.size.y);
			});
		}

		
		void detachWindow(){
			if(!targetWindow) return;

			(void)targetWindow->eventManager.erase<Window::ResizeEvent>(SwapChainName);

			targetWindow = nullptr;

			destroySurface();
		}

		void createSurface(VkInstance instance){
			if(surface) throw std::runtime_error("Surface window already set!");

			if(!targetWindow) throw std::runtime_error("Missing Window!");

			surface = targetWindow->createSurface(instance);
			this->instance = instance;
		}

		void destroySurface(){
			if(instance)vkDestroySurfaceKHR(instance, surface, nullptr);

			instance = nullptr;
			surface = nullptr;
		}

		~SwapChain(){
			if(device)vkDestroySwapchainKHR(device, swapChain, nullptr);

			cleanupSwapChain();
			detachWindow();
			destroySurface();
		}

		void cleanupSwapChain() const{
			if(!device)return;

			destroyOldSwapChain();
		}

		void recreate(){
			while (getTargetWindow()->iconified() || getTargetWindow()->getSize().area() == 0){
				getTargetWindow()->waitEvent();
			}

			vkDeviceWaitIdle(device);
			
			cleanupSwapChain();

			oldSwapChain = swapChain;

			createSwapChain(physicalDevice, device);

			recreateCallback(*this);
			resized = false;
		}

		[[nodiscard]] Window* getTargetWindow() const{ return targetWindow; }

		[[nodiscard]] VkExtent2D getExtent() const {return std::bit_cast<VkExtent2D>(targetWindow->getSize());}
		[[nodiscard]] auto size2D() const {return targetWindow->getSize();}

		[[nodiscard]] VkInstance getInstance() const{ return instance; }

		[[nodiscard]] VkSurfaceKHR getSurface() const{ return surface; }

		[[nodiscard]] explicit(false) operator VkSwapchainKHR() const noexcept{return swapChain;}

		auto getCurrentInFlightImage() const noexcept{
			return currentInFlightFrame;
		}

		void createSwapChain(VkPhysicalDevice physicalDevice, VkDevice device){
			this->device = device;
			this->physicalDevice = physicalDevice;

			const SwapChainInfo swapChainSupport(physicalDevice, surface);

			const VkSurfaceFormatKHR surfaceFormat = SwapChainInfo::chooseSwapSurfaceFormat(swapChainSupport.formats);

			const VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

			std::uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

			if(swapChainSupport.capabilities.maxImageCount){
				imageCount = std::min(imageCount, swapChainSupport.capabilities.maxImageCount);
			}

			VkSwapchainCreateInfoKHR createInfo{
					.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
					.surface = surface,
					.minImageCount = imageCount,
					.imageFormat = surfaceFormat.format,
					.imageColorSpace = surfaceFormat.colorSpace,
					.imageExtent = extent,
					.imageArrayLayers = 1,
					.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				};

			const QueueFamilyIndices indices(physicalDevice, surface);
			const std::array queueFamilyIndices{indices.graphicsFamily, indices.presentFamily};

			if(indices.graphicsFamily != indices.presentFamily){
				createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
				createInfo.queueFamilyIndexCount = 2;
				createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
			} else{
				createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				createInfo.queueFamilyIndexCount = 0;     // Optional
				createInfo.pQueueFamilyIndices = nullptr; // Optional
			}

			createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
			
			createInfo.oldSwapchain = this->oldSwapChain;

			//Set Window Alpha Blend Mode
			createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

			createInfo.presentMode = SwapChainInfo::chooseSwapPresentMode(swapChainSupport.presentModes);
			createInfo.clipped = true;

			if(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS){
				throw std::runtime_error("Failed to create swap chain!");
			}

			swapChainImageFormat = surfaceFormat.format;

			createImageViews();
		}

		void createImageViews(){
			auto [images, rst] = Util::enumerate(vkGetSwapchainImagesKHR, device, swapChain);
			swapChainImages.resize(images.size());

			for(const auto& [index, imageGroup] : swapChainImages | std::ranges::views::enumerate){
				imageGroup.image = images[index];
				imageGroup.imageView = ImageView(device, imageGroup.image, swapChainImageFormat);
			}
		}

		[[nodiscard]] VkDevice getDevice() const noexcept{ return device; }

		[[nodiscard]] VkFormat getFormat() const noexcept{ return swapChainImageFormat; }

		[[nodiscard]] auto getCurrentImageIndex() const noexcept{ return currentRenderingFrame; }

		[[nodiscard]] decltype(auto) getImageData() noexcept{
			return (swapChainImages);
		}

		[[nodiscard]] decltype(auto) getSwapChainImages() const noexcept{
			return swapChainImages | std::views::transform(&SwapChainFrameData::image);
		}

		[[nodiscard]] decltype(auto) getImageViews() const noexcept{
			return swapChainImages | std::views::transform(&SwapChainFrameData::imageView);
		}

		[[nodiscard]] decltype(auto) getFrameBuffers() const noexcept{
			return swapChainImages | std::views::transform(&SwapChainFrameData::framebuffer);
		}

		[[nodiscard]] decltype(auto) getFrameBuffers() noexcept{
			return swapChainImages | std::views::transform(&SwapChainFrameData::framebuffer);
		}

		[[nodiscard]] decltype(auto) getCommandFlushes() noexcept{
			return swapChainImages | std::views::transform(&SwapChainFrameData::commandFlush);
		}

		[[nodiscard]] std::uint32_t size() const noexcept{ return swapChainImages.size(); }

		std::uint32_t acquireNextImage(VkSemaphore semaphore, const std::uint64_t timeout = std::numeric_limits<std::uint64_t>::max()){
			std::uint32_t imageIndex{};

			const auto result = vkAcquireNextImageKHR(device, swapChain, timeout, semaphore, nullptr, &imageIndex);

			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				resized = true;
			} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
				throw std::runtime_error("failed to acquire swap chain image!");
			}

			currentRenderingFrame = imageIndex;
			return currentRenderingFrame;
		}

		void postImage(VkPresentInfoKHR& presentInfo){
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &swapChain;

			auto result = vkQueuePresentKHR(presentQueue, &presentInfo);

			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized) {
				recreate();
			} else if (result != VK_SUCCESS) {
				throw std::runtime_error("failed to present swap chain image!");
			}

			currentInFlightFrame = (currentInFlightFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		}

		[[nodiscard]] bool isResized() const noexcept{ return resized; }

	private:
		void destroyOldSwapChain() const{
			if(oldSwapChain)vkDestroySwapchainKHR(device, oldSwapChain, nullptr);
			oldSwapChain = nullptr;
		}


		[[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const{
			if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()){
				return capabilities.currentExtent;
			}

			auto size = targetWindow->getSize();

			size.clampX(capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			size.clampY(capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return std::bit_cast<VkExtent2D>(size);
		}

	public:
		SwapChain(const SwapChain& other) = delete;

		SwapChain(SwapChain&& other) noexcept = delete;

		SwapChain& operator=(const SwapChain& other) = delete;

		SwapChain& operator=(SwapChain&& other) noexcept = delete;
	};
}

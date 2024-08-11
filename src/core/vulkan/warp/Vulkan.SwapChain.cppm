module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.SwapChain;

import std;
import Core.Window;
import Core.Vulkan.Util.Invoker;
import Core.Vulkan.SwapChainInfo;
import Core.Vulkan.PhysicalDevice;

import Core.Vulkan.Image;

import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Buffer.FrameBuffer;

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

		//Move this to other place
		VkRenderPass renderPass{};

		struct SwapChainFrameData{
			VkImage image{};
			ImageView imageView{};
			FrameBuffer framebuffer{};
			CommandBuffer commandBuffer{};
		};

		std::vector<SwapChainFrameData> swapChainImages{};

		mutable bool resized{};

		void resize(int w, int h){
			resized = true;
		}

		std::uint32_t currentFrame{};

	public:
		std::function<void(SwapChain&)>  recreateCallback{};
		VkQueue presentQueue{};

		[[nodiscard]] SwapChain() = default;

		void attachWindow(Window* window){
			if(targetWindow) throw std::runtime_error("Target window already set!");

			targetWindow = window;

			targetWindow->eventManager.on<Window::ResizeEvent>(SwapChainName, [this](const auto& event){
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

			vkDestroyRenderPass(device, renderPass, nullptr);
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

		[[nodiscard]] VkInstance getInstance() const{ return instance; }

		[[nodiscard]] VkSurfaceKHR getSurface() const{ return surface; }

		[[nodiscard]] operator VkSwapchainKHR() const noexcept{return swapChain;}

		auto getCurrentRenderingImage() const noexcept{
			return currentFrame;
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
			createRenderPass();
			createSwapChainFramebuffers();
		}

		void createImageViews(){
			auto [images, rst] = Util::enumerate(vkGetSwapchainImagesKHR, device, swapChain);
			swapChainImages.resize(images.size());

			for(const auto& [index, imageGroup] : swapChainImages | std::ranges::views::enumerate){
				imageGroup.image = images[index];
				imageGroup.imageView = ImageView(device, imageGroup.image, swapChainImageFormat);
			}
		}

		[[nodiscard]] VkExtent2D getExtent() const noexcept{
			if(!targetWindow)return {};

			return std::bit_cast<VkExtent2D>(targetWindow->getSize().as<std::uint32_t>());
		}

		[[nodiscard]] VkDevice getDevice() const noexcept{ return device; }

		[[nodiscard]] VkFormat getFormat() const noexcept{ return swapChainImageFormat; }

		[[nodiscard]] decltype(auto) getSwapChainImages() const noexcept{
			return swapChainImages | std::views::transform(&SwapChainFrameData::image);
		}

		[[nodiscard]] decltype(auto) getImageViews() const noexcept{
			return swapChainImages | std::views::transform(&SwapChainFrameData::imageView);
		}

		[[nodiscard]] decltype(auto) getFrameBuffers() const noexcept{
			return swapChainImages | std::views::transform(&SwapChainFrameData::framebuffer);
		}

		[[nodiscard]] decltype(auto) getCommandBuffers() noexcept{
			return swapChainImages | std::views::transform(&SwapChainFrameData::commandBuffer);
		}

		[[nodiscard]] VkRenderPass getRenderPass() const noexcept{ return renderPass; }

		[[nodiscard]] std::uint32_t size() const noexcept{ return swapChainImages.size(); }

		std::uint32_t acquireNextImage(VkSemaphore semaphore, const std::uint64_t timeout = std::numeric_limits<std::uint64_t>::max()){
			std::uint32_t imageIndex{};

			const auto result = vkAcquireNextImageKHR(device, swapChain, timeout, semaphore, nullptr, &imageIndex);

			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				recreate();
			} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
				throw std::runtime_error("failed to acquire swap chain image!");
			}

			return imageIndex;
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

			currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		}

	private:
		void destroyOldSwapChain() const{
			if(oldSwapChain)vkDestroySwapchainKHR(device, oldSwapChain, nullptr);
			oldSwapChain = nullptr;
		}

		void createSwapChainFramebuffers(){
			for(const auto& [i, group] : swapChainImages | std::views::enumerate){
				group.framebuffer = FrameBuffer{device, targetWindow->getSize(), renderPass, std::array{group.imageView.get()}};
			}
		}

		void createRenderPass(){
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = swapChainImageFormat;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference colorAttachmentRef{};
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;

			VkSubpassDependency dependency{};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &colorAttachment;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 1;
			renderPassInfo.pDependencies = &dependency;

			if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS){
				throw std::runtime_error("failed to create render pass!");
			}
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

module;

#include <vulkan/vulkan.h>


export module Core.Vulkan.SwapChain;

import std;
import Core.Window;
import Core.Vulkan.Util.Invoker;
import Core.Vulkan.SwapChainInfo;
import Core.Vulkan.PhysicalDevice;
import Core.Vulkan.Image;
import Core.Vulkan.LogicalDevice;

export namespace Core::Vulkan{
	class SwapChain{
	public:
		static constexpr std::string_view SwapChainName{"SwapChain"};

	private:
		Window* targetWindow{};
		VkInstance instance{};
		VkDevice device{};
		VkSurfaceKHR surface{};

		VkSwapchainKHR swapChain{};

		VkFormat swapChainImageFormat{};

		VkRenderPass renderPass{};

		struct SwapChainImage{
			VkImage image{};
			VkImageView imageView{};
			VkFramebuffer framebuffer{};
		};

		std::vector<SwapChainImage> swapChainImages{};
		// std::vector<VkImage> swapChainImages{};
		// std::vector<VkImageView> swapChainImageViews{};
		// std::vector<VkFramebuffer> swapChainFramebuffers{};


	public:
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
			if(surface && instance) vkDestroySurfaceKHR(instance, surface, nullptr);

			instance = nullptr;
			surface = nullptr;
		}

		~SwapChain(){
			cleanupSwapChain();
			detachWindow();
		}

		void cleanupSwapChain() {
			if(!device)return;

			for (const auto& framebuffer : getFrameBuffers()) {
				vkDestroyFramebuffer(device, framebuffer, nullptr);
			}

			vkDestroyRenderPass(device, renderPass, nullptr);

			for (const auto view : getImageViews()) {
				vkDestroyImageView(device, view, nullptr);
			}

			vkDestroySwapchainKHR(device, swapChain, nullptr);

			device = nullptr;
		}

		[[nodiscard]] Window* getTargetWindow() const{ return targetWindow; }

		[[nodiscard]] VkInstance getInstance() const{ return instance; }

		[[nodiscard]] VkSurfaceKHR getSurface() const{ return surface; }

		[[nodiscard]] operator VkSwapchainKHR() const noexcept{return swapChain;}

		void resize(int w, int h){}

		void createSwapChain(VkPhysicalDevice vkPhysicalDevice, VkDevice device){
			const SwapChainInfo swapChainSupport(vkPhysicalDevice, surface);

			const VkSurfaceFormatKHR surfaceFormat = SwapChainInfo::chooseSwapSurfaceFormat(swapChainSupport.formats);

			const VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

			std::uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
			if(swapChainSupport.capabilities.maxImageCount && imageCount > swapChainSupport.capabilities.maxImageCount){
				imageCount = swapChainSupport.capabilities.maxImageCount;
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

			const QueueFamilyIndices indices(vkPhysicalDevice, surface);
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

			//Set Window Alpha Blend Mode
			createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

			createInfo.presentMode = SwapChainInfo::chooseSwapPresentMode(swapChainSupport.presentModes);
			createInfo.clipped = true;

			createInfo.oldSwapchain = nullptr;

			if(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS){
				throw std::runtime_error("Failed to create swap chain!");
			}

			swapChainImageFormat = surfaceFormat.format;
			this->device = device;

			auto [images, rst] = Util::enumerate(vkGetSwapchainImagesKHR, device, swapChain);
			swapChainImages.resize(images.size());

			for(const auto& [index, imageGroup] : swapChainImages | std::ranges::views::enumerate){
				imageGroup.image = images[index];
				imageGroup.imageView = createImageView(device, imageGroup.image, swapChainImageFormat, 1);
			}

			createRenderPass();
			createSwapChainFramebuffers();
		}

		[[nodiscard]] VkExtent2D getExtent() const noexcept{
			if(!targetWindow)return {};

			return std::bit_cast<VkExtent2D>(targetWindow->getSize().as<std::uint32_t>());
		}

		[[nodiscard]] VkDevice getDevice() const noexcept{ return device; }

		[[nodiscard]] VkFormat getFormat() const noexcept{ return swapChainImageFormat; }

		[[nodiscard]] decltype(auto) getSwapChainImages() const noexcept{
			return swapChainImages | std::views::transform(&SwapChainImage::image);
		}

		[[nodiscard]] decltype(auto) getImageViews() const noexcept{
			return swapChainImages | std::views::transform(&SwapChainImage::imageView);
		}

		[[nodiscard]] decltype(auto) getFrameBuffers() const noexcept{
			return swapChainImages | std::views::transform(&SwapChainImage::framebuffer);
		}

		[[nodiscard]] VkRenderPass getRenderPass() const noexcept{ return renderPass; }

		[[nodiscard]] std::uint32_t size() const noexcept{ return swapChainImages.size(); }

	private:

		void createSwapChainFramebuffers(){
			for(const auto& [i, group] : swapChainImages | std::views::enumerate){
				std::array attachments{group.imageView};

				VkFramebufferCreateInfo framebufferInfo{};

				framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferInfo.renderPass = renderPass;
				framebufferInfo.attachmentCount = attachments.size();
				framebufferInfo.pAttachments = attachments.data();
				framebufferInfo.width = targetWindow->getSize().x;
				framebufferInfo.height = targetWindow->getSize().y;
				framebufferInfo.layers = 1;

				if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &group.framebuffer) != VK_SUCCESS){
					throw std::runtime_error("Failed to create framebuffer!");
				}
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

			auto size = targetWindow->getSize().as<std::uint32_t>();

			size.clampX(capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			size.clampY(capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return std::bit_cast<VkExtent2D>(size);
		}
	};
}

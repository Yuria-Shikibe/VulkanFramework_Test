module;

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

export module Core.Vulkan.Manager;

import ext.MetaProgramming;
import Core.Window;
import Core.Vulkan.Uniform;
import Core.Vulkan.Shader;
import Core.Vulkan.Buffer;
import std;

export namespace Core::Vulkan{
	void getValidExtensions(){
		std::uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		std::println("available extensions:");

		for(const auto& extension : extensions){
			std::println("{}", extension.extensionName);
		}
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData){
		std::println(std::cerr, "validation layer: {}", pCallbackData->pMessage);

		return VK_FALSE;
	}

	VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pCallback){
		const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
			instance, "vkCreateDebugUtilsMessengerEXT"));

		if(func != nullptr){
			return func(instance, pCreateInfo, pAllocator, pCallback);
		} else{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void DestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT callback,
		const VkAllocationCallbacks* pAllocator
	){
		const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
			instance, "vkDestroyDebugUtilsMessengerEXT"));
		if(func != nullptr){
			func(instance, callback, pAllocator);
		}
	}



	std::vector<const char*> getGLFW_Extensions(){
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		return {glfwExtensions, glfwExtensions + glfwExtensionCount};
	}

	constexpr bool EnableValidationLayers{DEBUG_CHECK};

	std::vector<const char*> getRequiredExtensions(){
		auto extensions = getGLFW_Extensions();

		if constexpr(EnableValidationLayers){
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	constexpr int MAX_FRAMES_IN_FLIGHT = 2;

	struct QueueFamilyIndices{
		static constexpr auto InvalidFamily = std::numeric_limits<std::uint32_t>::max();
		std::uint32_t graphicsFamily = InvalidFamily;
		std::uint32_t presentFamily = InvalidFamily;

		[[nodiscard]] constexpr bool isComplete() const noexcept{
			return graphicsFamily != InvalidFamily && presentFamily != InvalidFamily;
		}
	};

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface){
		QueueFamilyIndices indices;

		std::uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());


		for(const auto& [index, queueFamily] : queueFamilies | std::views::enumerate){
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &presentSupport);

			if(queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT){
				indices.graphicsFamily = static_cast<int>(index);
			}

			if(queueFamily.queueCount > 0 && presentSupport){
				indices.presentFamily = index;
			}


			if(indices.isComplete()){
				break;
			}
		}


		return indices;
	}

	struct PhysicalDevice{
		VkPhysicalDevice device{}; // = VK_NULL_HANDLE;

		explicit constexpr operator bool() const noexcept{
			return device != nullptr;
		}

		[[nodiscard]] std::string getName() const{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(device, &deviceProperties);

			return deviceProperties.deviceName;
		}

		[[nodiscard]] std::uint32_t rateDeviceSuitability() const{
			// Application can't function without geometry shaders
			//Minimum Requirements
			if(!meetFeatures(&VkPhysicalDeviceFeatures::geometryShader)){
				return 0;
			}

			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(device, &deviceProperties);

			std::uint32_t score = 0;

			// Discrete GPUs have a significant performance advantage
			if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
				score += 1000;
			}

			// Maximum possible size of textures affects graphics quality
			score += deviceProperties.limits.maxImageDimension2D;

			return score;
		}

		template <typename... Args>
			requires requires(VkPhysicalDeviceFeatures features, Args... args){
				requires (std::same_as<VkBool32, typename ext::GetMemberPtrInfo<Args>::ValueType> && ...);
				requires std::conjunction_v<std::is_member_object_pointer<Args>...>;
			}
		[[nodiscard]] bool meetFeatures(Args... args) const{
			VkPhysicalDeviceFeatures deviceFeatures;
			vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

			return (static_cast<bool>(deviceFeatures.*args) && ...);
		}

		[[nodiscard]] constexpr operator VkPhysicalDevice() const noexcept{
			return device;
		}
	};

	struct SwapChainInfo{
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats{};
		std::vector<VkPresentModeKHR> presentModes{};
	};

	SwapChainInfo querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface){
		SwapChainInfo details{};

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		std::uint32_t formatCount{};
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if(formatCount != 0){
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		std::uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if(presentModeCount != 0){
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}


		return details;
	}

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats){
		static constexpr VkFormat TARGET_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
		static constexpr VkColorSpaceKHR TARGET_VK_COLOR_SPACE = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		if(availableFormats.size() == 1 && availableFormats.front().format == VK_FORMAT_UNDEFINED){
			return {TARGET_FORMAT, TARGET_VK_COLOR_SPACE};
		}

		for(const auto& availableFormat : availableFormats){
			if(availableFormat.format == TARGET_FORMAT && availableFormat.colorSpace == TARGET_VK_COLOR_SPACE){
				return availableFormat;
			}
		}

		return availableFormats.front();
	}

	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes){
		static constexpr VkPresentModeKHR TARGET_MODE = VK_PRESENT_MODE_MAILBOX_KHR;

		VkPresentModeKHR fallBack = VK_PRESENT_MODE_FIFO_KHR;

		for(const auto& availablePresentMode : availablePresentModes){
			if(availablePresentMode == TARGET_MODE){
				return availablePresentMode;
			}

			if(availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR){
				fallBack = availablePresentMode;
			}
		}
		return fallBack;
	}

	class VulkanManager{
	public:
		[[nodiscard]] explicit VulkanManager(GLFWwindow* window) : window{window}{
			initVulkan(window);
		}

		~VulkanManager(){
			cleanup();
		}

		VkApplicationInfo defaultAppInfo = {
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pApplicationName = "Hello Triangle",
				.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
				.pEngineName = "No Engine",
				.engineVersion = VK_MAKE_VERSION(1, 0, 0),
				.apiVersion = VK_API_VERSION_1_0,
			};

		//TODO move these to global
		const std::vector<const char*> validationLayers = {
				"VK_LAYER_KHRONOS_validation"
			};

		const std::vector<const char*> deviceExtensions = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME
			};

		VkDebugUtilsMessengerEXT callback{};

		VkInstance instance{};
		PhysicalDevice physicalDevice{}; // = VK_NULL_HANDLE;


		VkDevice device{};

		VkQueue graphicsQueue{};
		VkQueue presentQueue{};

		GLFWwindow* window;
		VkSurfaceKHR surface{};
		VkSwapchainKHR swapChain{};
		std::vector<VkImage> swapChainImages{};
		std::vector<VkImageView> swapChainImageViews{};
		VkFormat swapChainImageFormat{};
		VkExtent2D swapChainExtent{};
		std::vector<VkFramebuffer> swapChainFramebuffers{};


		VkRenderPass renderPass{};
		VkPipelineLayout pipelineLayout{};

		VkDescriptorSetLayout descriptorSetLayout{};
		VkPipeline graphicsPipeline{};


		VkCommandPool commandPool{};
		std::vector<VkCommandBuffer> commandBuffers{};


		std::vector<VkFence> inFlightFences{};
		// std::vector<VkFence> imagesInFlight{};

		std::size_t currentFrame = 0;
		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;

		DataBuffer vertexBuffer{};
		DataBuffer indexBuffer{};
		std::vector<DataBuffer> uniformBuffers{};

		VkDescriptorPool descriptorPool{};
		std::vector<VkDescriptorSet> descriptorSets{};

		bool framebufferResized = false;

		[[nodiscard]] bool checkValidationLayerSupport() const{
			std::uint32_t layerCount;
			vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

			std::vector<VkLayerProperties> availableLayers(layerCount);
			vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

			std::unordered_set<std::string> requiredLayers(validationLayers.begin(), validationLayers.end());

			for(const auto& extension : availableLayers){
				requiredLayers.erase(extension.layerName);
			}

			return requiredLayers.empty();
		}

		[[nodiscard]] bool checkDeviceExtensionSupport(VkPhysicalDevice device) const{
			std::uint32_t extensionCount;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

			std::vector<VkExtensionProperties> availableExtensions(extensionCount);
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

			std::unordered_set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

			for(const auto& extension : availableExtensions){
				requiredExtensions.erase(extension.extensionName);
			}

			return requiredExtensions.empty();
		}

		void updateUniformBuffer(const std::uint32_t currentImage) {
			static auto startTime = std::chrono::high_resolution_clock::now();

			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration<float>(currentTime - startTime).count();


			auto data = uniformBuffers[currentImage].map();
			UniformBufferObject d{{}, 0.3f, 0.4F, 0.5};
			std::memcpy(data, &d, sizeof(d));
			uniformBuffers[currentImage].unmap();
		}

		void drawFrame(){
			vkWaitForFences(device, 1, &inFlightFences[currentFrame], true, std::numeric_limits<uint64_t>::max());
			vkResetFences(device, 1, &inFlightFences[currentFrame]);

			std::uint32_t imageIndex{};
			VkResult result = vkAcquireNextImageKHR(device, swapChain,
				std::numeric_limits<std::uint64_t>::max(), imageAvailableSemaphores[currentFrame], nullptr, &imageIndex);

			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				recreateSwapChain();
				return;
			} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
				throw std::runtime_error("failed to acquire swap chain image!");
			}

			updateUniformBuffer(imageIndex);

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			const std::array waitSemaphores{imageAvailableSemaphores[currentFrame]};
			constexpr VkPipelineStageFlags waitStages[]{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
			submitInfo.waitSemaphoreCount = waitSemaphores.size();
			submitInfo.pWaitSemaphores = waitSemaphores.data();
			submitInfo.pWaitDstStageMask = waitStages;

			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

			const std::array signalSemaphores{renderFinishedSemaphores[currentFrame]};
			submitInfo.signalSemaphoreCount = signalSemaphores.size();
			submitInfo.pSignalSemaphores = signalSemaphores.data();

			vkResetFences(device, 1, &inFlightFences[currentFrame]);

			if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
				throw std::runtime_error("failed to submit draw command buffer!");
			}

			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

			presentInfo.waitSemaphoreCount = signalSemaphores.size();
			presentInfo.pWaitSemaphores = signalSemaphores.data();

			const std::array swapChains{swapChain};
			presentInfo.swapchainCount = swapChains.size();
			presentInfo.pSwapchains = swapChains.data();
			presentInfo.pImageIndices = &imageIndex;
			presentInfo.pResults = nullptr; // Optional


			result = vkQueuePresentKHR(presentQueue, &presentInfo);

			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
				framebufferResized = false;
				recreateSwapChain();
			} else if (result != VK_SUCCESS) {
				throw std::runtime_error("failed to present swap chain image!");
			}

			currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		}


	private:
		void initVulkan(GLFWwindow* window){
			// vkCreateInstance()

			createInstance();
			setupDebugCallback();

			initWindow(window);

			selectPhysicalDevice();
			createLogicalDevice();

			createSwapChain();
			createImageViews();

			createRenderPass();

			descriptorSetLayout = createDescriptorSetLayout(device);
			createGraphicsPipeline();

			createSwapChainFramebuffers();

			createCommandPool();

			vertexBuffer = createVertexBuffer(physicalDevice.device, device, commandPool, graphicsQueue);
			indexBuffer = createIndexBuffer(physicalDevice.device, device, commandPool, graphicsQueue);

			createDescriptorPool();
			createUniformBuffer();
			createDescriptorSets();

			createCommandBuffers();
			createSyncObjects();

			std::cout.flush();
		}

		void createDescriptorSets() {
			std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayout);
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = swapChainImages.size();
			allocInfo.pSetLayouts = layouts.data();

			descriptorSets.resize(swapChainImages.size());
			if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
				throw std::runtime_error("failed to allocate descriptor sets!");
			}

			for (size_t i = 0; i < swapChainImages.size(); i++) {
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = uniformBuffers[i];
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(UniformBufferObject);

				VkWriteDescriptorSet descriptorWrite{};
				descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrite.dstSet = descriptorSets[i];
				descriptorWrite.dstBinding = 0;
				descriptorWrite.dstArrayElement = 0;
				descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrite.descriptorCount = 1;
				descriptorWrite.pBufferInfo = &bufferInfo;

				vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
			}
		}

		void createDescriptorPool() {
			VkDescriptorPoolSize poolSize{};
			poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			poolSize.descriptorCount = static_cast<uint32_t>(swapChainImages.size());

			VkDescriptorPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.poolSizeCount = 1;
			poolInfo.pPoolSizes = &poolSize;
			poolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());

			if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
				throw std::runtime_error("failed to create descriptor pool!");
			}
		}

		void createUniformBuffer() {
			VkDeviceSize bufferSize = sizeof(UniformBufferObject);

			uniformBuffers.resize(swapChainImages.size());

			for (auto& buffer : uniformBuffers) {

				buffer = DataBuffer{
					physicalDevice, device, bufferSize,
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
				};
			}
		}

		void recreateSwapChain() {
			int width = 0, height = 0;
			while (width == 0 || height == 0) {
				glfwGetFramebufferSize(window, &width, &height);
				glfwWaitEvents();
			}

			vkDeviceWaitIdle(device);

			cleanupSwapChain();


			createSwapChain();
			createImageViews();
			createRenderPass();
			createGraphicsPipeline();
			createSwapChainFramebuffers();
			createCommandBuffers();
		}

		void createSyncObjects(){
			imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
			renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
			inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

			VkSemaphoreCreateInfo semaphoreInfo = {};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo fenceInfo = {};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			for(std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
				if(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
					vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
					vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS){
					throw std::runtime_error("failed to create synchronization objects for a frame!");
				}
			}
		}

		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities){
			if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()){
				return capabilities.currentExtent;
			} else{
				int width, height;
				glfwGetFramebufferSize(window, &width, &height);

				VkExtent2D actualExtent = {
					static_cast<std::uint32_t>(width),
					static_cast<std::uint32_t>(height)
				};

				actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
												capabilities.maxImageExtent.width);
				actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
												 capabilities.maxImageExtent.height);

				return actualExtent;
			}
		}

		void createCommandBuffers(){
			commandBuffers.resize(swapChainFramebuffers.size());

			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = commandPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size());

			if(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS){
				throw std::runtime_error("Failed to allocate command buffers!");
			}

			for(const auto& [i, commandBuffer] : commandBuffers | std::views::enumerate){
				VkCommandBufferBeginInfo beginInfo = {};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				beginInfo.pInheritanceInfo = nullptr; // Optional

				if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS){
					throw std::runtime_error("failed to begin recording command buffer!");
				}

				VkRenderPassBeginInfo renderPassInfo = {};
				renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassInfo.renderPass = renderPass;
				renderPassInfo.framebuffer = swapChainFramebuffers[i];

				renderPassInfo.renderArea.offset = {0, 0};
				renderPassInfo.renderArea.extent = swapChainExtent;

				VkClearValue clearColor = {0.4f, 0.4f, 0.4f, 1.0f};
				renderPassInfo.clearValueCount = 1;
				renderPassInfo.pClearValues = &clearColor;

				vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

				const VkBuffer vertexBuffers[]{vertexBuffer.getHandler()};

				const VkDeviceSize offsets[]{0};

				VkViewport viewport{};
				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = static_cast<float>(swapChainExtent.width);
				viewport.height = static_cast<float>(swapChainExtent.height);
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;
				vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

				VkRect2D scissor{};
				scissor.offset = {0, 0};
				scissor.extent = swapChainExtent;
				vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

				vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

				vkCmdBindDescriptorSets(commandBuffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0,
                        nullptr);


				vkCmdDrawIndexed(commandBuffer,
					static_cast<std::uint32_t>(test_indices.size()), 1, 0, 0, 0);


				vkCmdEndRenderPass(commandBuffer);

				if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS){
					throw std::runtime_error("Failed to record command buffer!");
				}
			}

		}

		void createCommandPool(){
			QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice.device, surface);

			VkCommandPoolCreateInfo poolInfo = {};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
			poolInfo.flags = 0; // Optional

			if(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS){
				throw std::runtime_error("Failed to create command pool!");
			}
		}

		void createSwapChainFramebuffers(){
			swapChainFramebuffers.resize(swapChainImageViews.size());
			for(const auto& [i, view] : swapChainImageViews | std::views::enumerate){
				std::array attachments{view};

				VkFramebufferCreateInfo framebufferInfo{};
				framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferInfo.renderPass = renderPass;
				framebufferInfo.attachmentCount = attachments.size();
				framebufferInfo.pAttachments = attachments.data();
				framebufferInfo.width = swapChainExtent.width;
				framebufferInfo.height = swapChainExtent.height;
				framebufferInfo.layers = 1;

				if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS){
					throw std::runtime_error("failed to create framebuffer!");
				}
			}
		}

		void createGraphicsPipeline(){
			ShaderModule vertShaderModule{
					device, R"(D:\projects\vulkan_framework\properties\shader\spv\test.vert.spv)"
				};
			ShaderModule fragShaderModule{
					device, R"(D:\projects\vulkan_framework\properties\shader\spv\test.frag.spv)"
				};

			VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
			vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertShaderStageInfo.module = vertShaderModule;
			vertShaderStageInfo.pName = "main";

			VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
			fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragShaderStageInfo.module = fragShaderModule;
			fragShaderStageInfo.pName = "main";

			VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

			VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
			vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

			auto bindingDescription = Vertex::getBindingDescription();
			auto attributeDescriptions = Vertex::getAttributeDescriptions();

			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptions.size());
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
			vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

			VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			inputAssembly.primitiveRestartEnable = VK_FALSE;

			VkPipelineViewportStateCreateInfo viewportState{};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;

			VkPipelineRasterizationStateCreateInfo rasterizer{};
			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizer.depthClampEnable = VK_FALSE;
			rasterizer.rasterizerDiscardEnable = VK_FALSE;
			rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizer.lineWidth = 1.0f;

			rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

			rasterizer.depthBiasEnable = VK_FALSE;

			VkPipelineMultisampleStateCreateInfo multisampling{};
			multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling.sampleShadingEnable = VK_FALSE;
			multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			VkPipelineColorBlendAttachmentState colorBlendAttachment{};
			colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachment.blendEnable = VK_FALSE;

			VkPipelineColorBlendStateCreateInfo colorBlending{};
			colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlending.logicOpEnable = VK_FALSE;
			colorBlending.logicOp = VK_LOGIC_OP_COPY;
			colorBlending.attachmentCount = 1;
			colorBlending.pAttachments = &colorBlendAttachment;
			colorBlending.blendConstants[0] = 0.0f;
			colorBlending.blendConstants[1] = 0.0f;
			colorBlending.blendConstants[2] = 0.0f;
			colorBlending.blendConstants[3] = 0.0f;

			std::vector<VkDynamicState> dynamicStates = {
					VK_DYNAMIC_STATE_VIEWPORT,
					VK_DYNAMIC_STATE_SCISSOR
				};
			VkPipelineDynamicStateCreateInfo dynamicState{};
			dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
			dynamicState.pDynamicStates = dynamicStates.data();


			VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = 1;
			pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

			if(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS){
				throw std::runtime_error("failed to create pipeline layout!");
			}

			VkGraphicsPipelineCreateInfo pipelineInfo{};
			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.stageCount = 2;
			pipelineInfo.pStages = shaderStages;
			pipelineInfo.pVertexInputState = &vertexInputInfo;
			pipelineInfo.pInputAssemblyState = &inputAssembly;
			pipelineInfo.pViewportState = &viewportState;
			pipelineInfo.pRasterizationState = &rasterizer;
			pipelineInfo.pMultisampleState = &multisampling;
			pipelineInfo.pColorBlendState = &colorBlending;
			pipelineInfo.pDynamicState = &dynamicState;
			pipelineInfo.layout = pipelineLayout;
			pipelineInfo.renderPass = renderPass;
			pipelineInfo.subpass = 0;
			pipelineInfo.basePipelineHandle = nullptr;

			if(vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &graphicsPipeline) !=
				VK_SUCCESS){
				throw std::runtime_error("failed to create graphics pipeline!");
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

			VkRenderPassCreateInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
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

		bool isPhysicalDeviceValid(VkPhysicalDevice device) const{
			const QueueFamilyIndices indices = findQueueFamilies(device, surface);

			const bool extensionsSupported = checkDeviceExtensionSupport(device);

			bool swapChainAdequate = false;
			if(extensionsSupported){
				const SwapChainInfo swapChainSupport = querySwapChainSupport(device, surface);
				swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
			}

			return indices.isComplete() && extensionsSupported && swapChainAdequate;
		}

		void initWindow(GLFWwindow* window){
			if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS){
				throw std::runtime_error("failed to create window surface!");
			}
		}

		void createLogicalDevice(){
			const QueueFamilyIndices indices = findQueueFamilies(physicalDevice.device, surface);


			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
			const std::unordered_set<std::uint32_t> uniqueQueueFamilies = {
					indices.graphicsFamily, indices.presentFamily
				};

			const float queuePriority = 1.0f;
			for(const std::uint32_t queueFamily : uniqueQueueFamilies){
				VkDeviceQueueCreateInfo queueCreateInfo = {};
				queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueCreateInfo.queueFamilyIndex = queueFamily;
				queueCreateInfo.queueCount = 1;
				queueCreateInfo.pQueuePriorities = &queuePriority;
				queueCreateInfos.push_back(queueCreateInfo);
			}

			VkDeviceQueueCreateInfo queueCreateInfo{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = indices.graphicsFamily,
					.queueCount = 1,
				};

			queueCreateInfo.pQueuePriorities = &queuePriority;

			VkPhysicalDeviceFeatures deviceFeatures = {};

			VkDeviceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.pEnabledFeatures = &deviceFeatures;

			createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
			createInfo.pQueueCreateInfos = queueCreateInfos.data();

			createInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
			createInfo.ppEnabledExtensionNames = deviceExtensions.data();


			if constexpr(EnableValidationLayers){
				createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
				createInfo.ppEnabledLayerNames = validationLayers.data();
			} else{
				createInfo.enabledLayerCount = 0;
			}

			if(vkCreateDevice(physicalDevice.device, &createInfo, nullptr, &device) != VK_SUCCESS){
				throw std::runtime_error("Failed to create logical device!");
			}

			vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
			vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
		}

		void setupDebugCallback(){
			if constexpr(!EnableValidationLayers) return;

			const VkDebugUtilsMessengerCreateInfoEXT createInfo{
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
					.messageSeverity =
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
					.messageType =
					VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
					.pfnUserCallback = debugCallback,
					.pUserData = nullptr
				};

			const auto rst = CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &callback);
			if(rst != VK_SUCCESS){
				throw std::runtime_error("failed to set up debug callback!");
			} else{
				std::println("Debug Callback Setup Succeed");
			}
		}

		void createInstance(){
			VkInstanceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			createInfo.pApplicationInfo = &defaultAppInfo;

			const auto rst = getGLFW_Extensions();

			createInfo.enabledExtensionCount = rst.size();
			createInfo.ppEnabledExtensionNames = rst.data();

			if constexpr(EnableValidationLayers){
				if(!checkValidationLayerSupport()){
					throw std::runtime_error("validation layers requested, but not available!");
				} else{
					std::println("Using Validation Layer: ");

					for(const auto& validationLayer : validationLayers){
						std::println("\t{}", validationLayer);
					}
				}

				createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
				createInfo.ppEnabledLayerNames = validationLayers.data();
			} else{
				createInfo.enabledLayerCount = 0;
			}

			const auto extensions = getRequiredExtensions();
			createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
			createInfo.ppEnabledExtensionNames = extensions.data();

			if(vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS){
				throw std::runtime_error("failed to create vulkan instance!");
			} else{
				std::println("Init VK Instance Succeed: 0x{:x}", reinterpret_cast<std::uintptr_t>(instance));
			}
		}

		void selectPhysicalDevice(){
			std::uint32_t deviceCount = 0;
			vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

			if(deviceCount == 0){
				throw std::runtime_error("Failed to find GPUs with Vulkan support!");
			}

			std::vector<VkPhysicalDevice> devices(deviceCount);
			vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

			// Use an ordered map to automatically sort candidates by increasing score
			std::multimap<std::uint32_t, PhysicalDevice, std::greater<std::uint32_t>> candidates;

			for(const auto& device : devices | std::ranges::views::transform([](auto& device){
				return PhysicalDevice{device};
			})){
				auto score = device.rateDeviceSuitability();
				candidates.insert(std::make_pair(score, device));
			}

			// Check if the best candidate is suitable at all
			for(const auto& device : candidates
			    | std::views::filter(
				    [this](const decltype(candidates)::value_type& item){
					    return item.first && isPhysicalDeviceValid(item.second.device);
				    })
			    | std::views::values){
				physicalDevice = device;
				break;
			}

			if(!physicalDevice){
				throw std::runtime_error("Failed to find a suitable GPU!");
			} else{
				std::println("Selected Physical Device: {}", physicalDevice.getName());
			}
		}

		void createSwapChain(){
			const SwapChainInfo swapChainSupport = querySwapChainSupport(physicalDevice.device, surface);

			const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
			const VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
			const VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

			std::uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
			if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.
				maxImageCount){
				imageCount = swapChainSupport.capabilities.maxImageCount;
			}

			VkSwapchainCreateInfoKHR createInfo{
					.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
					// .pNext = ,
					// .flags = ,
					.surface = surface,
					.minImageCount = imageCount,
					.imageFormat = surfaceFormat.format,
					.imageColorSpace = surfaceFormat.colorSpace,
					.imageExtent = extent,
					.imageArrayLayers = 1,
					.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
					// .imageSharingMode = ,
					// .queueFamilyIndexCount = ,
					// .pQueueFamilyIndices = ,
					// .preTransform = ,
					// .compositeAlpha = ,
					// .presentMode = ,
					// .clipped = ,
					// .oldSwapchain =
				};

			const QueueFamilyIndices indices = findQueueFamilies(physicalDevice.device, surface);
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

			createInfo.presentMode = presentMode;
			createInfo.clipped = true;

			createInfo.oldSwapchain = nullptr;

			if(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS){
				throw std::runtime_error("Failed to create swap chain!");
			}

			vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
			swapChainImages.resize(imageCount);
			vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

			swapChainImageFormat = surfaceFormat.format;
			swapChainExtent = extent;
		}

		void createImageViews(){
			swapChainImageViews.resize(swapChainImages.size());
			for(const auto& [index, swapChainImage] : swapChainImages | std::ranges::views::enumerate){
				VkImageViewCreateInfo createInfo = {};
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = swapChainImage;
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = swapChainImageFormat;
				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;

				if(vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[index]) != VK_SUCCESS){
					throw std::runtime_error("failed to create image views!");
				}
			}
		}


		void cleanupSwapChain() {
			for (const auto& framebuffer : swapChainFramebuffers) {
				vkDestroyFramebuffer(device, framebuffer, nullptr);
			}

			vkFreeCommandBuffers(device, commandPool,
			static_cast<std::uint32_t>(commandBuffers.size()), commandBuffers.data());

			vkDestroyPipeline(device, graphicsPipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyRenderPass(device, renderPass, nullptr);

			for (const auto view : swapChainImageViews) {
				vkDestroyImageView(device, view, nullptr);
			}

			vkDestroySwapchainKHR(device, swapChain, nullptr);
		}

		void cleanup(){
			cleanupSwapChain();

			vkDestroyDescriptorPool(device, descriptorPool, nullptr);

			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

			indexBuffer.destroy();
			vertexBuffer.destroy();
			uniformBuffers.clear();

			for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
				vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
				vkDestroyFence(device, inFlightFences[i], nullptr);
			}

			vkDestroyCommandPool(device, commandPool, nullptr);

			vkDestroyDevice(device, nullptr);

			vkDestroySurfaceKHR(instance, surface, nullptr);

			if constexpr(EnableValidationLayers){
				DestroyDebugUtilsMessengerEXT(instance, callback, nullptr);
			}

			vkDestroyInstance(instance, nullptr);
		}

	};
}

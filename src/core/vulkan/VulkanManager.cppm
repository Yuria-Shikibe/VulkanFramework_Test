module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Manager;

import ext.MetaProgramming;

import Core.Window;
import Core.Vulkan.Uniform;
import Core.Vulkan.Shader;
import Core.Vulkan.Image;
import Core.Vulkan.Vertex;
import std;

import Core.Vulkan.Core;
import Core.Vulkan.Validation;
import Core.Vulkan.SwapChain;
import Core.Vulkan.PhysicalDevice;
import Core.Vulkan.LogicalDevice;
import Core.Vulkan.DescriptorSet;
import Core.Vulkan.PipelineLayout;

import Core.Vulkan.Fence;
import Core.Vulkan.Semaphore;

import Core.Vulkan.Buffer.ExclusiveBuffer;
import Core.Vulkan.Buffer.UniformBuffer;

import Core.Vulkan.Preinstall;

import Core.Vulkan.Pipeline;

import Core.Vulkan.CommandPool;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Sampler;


export namespace Core::Vulkan{
	class VulkanManager{
	public:
		[[nodiscard]] explicit VulkanManager(Window* window){
			initVulkan(window);
		}

		~VulkanManager(){
			vkDestroyImageView(context.device, textureImageView, nullptr);
			vkDestroyImage(context.device, textureImage, nullptr);
			vkFreeMemory(context.device, textureImageMemory, nullptr);
		}

		Context context{};

		CommandPool commandPool{};
		DescriptorSetPool descriptorPool{};

		SwapChain swapChain{};

		std::vector<Fence> inFlightFences{};
		std::vector<Semaphore> imageAvailableSemaphores{};
		std::vector<Semaphore> renderFinishedSemaphores{};

		DescriptorSetLayout descriptorSetLayout{};

		PipelineLayout pipelineLayout{};
		GraphicPipeline graphicsPipeline{};


		ExclusiveBuffer vertexBuffer{};
		ExclusiveBuffer indexBuffer{};
		std::vector<UniformBuffer> uniformBuffers{};

		std::vector<DescriptorSet> descriptorSets{};

		std::uint32_t mipLevels{};
		VkImage textureImage{};
		VkDeviceMemory textureImageMemory{};
		VkImageView textureImageView{};
		Sampler textureSampler{};

		bool framebufferResized = false;

		void updateUniformBuffer(const std::uint32_t currentImage) {
			static auto startTime = std::chrono::high_resolution_clock::now();

			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration<float>(currentTime - startTime).count();


			auto data = uniformBuffers[currentImage].memory.map();
			Geom::Matrix3D matrix3D{};
			matrix3D.setToRotation(time * 5);
			new (data) UniformBufferObject{matrix3D, 0.1f};
			uniformBuffers[currentImage].memory.unmap();
		}

		void drawFrame(){
			auto currentFrame = swapChain.getCurrentRenderingImage();

			inFlightFences[currentFrame].wait();
			inFlightFences[currentFrame].reset();

			const auto imageIndex = swapChain.acquireNextImage(imageAvailableSemaphores[currentFrame]);

			updateUniformBuffer(imageIndex);

			const std::array signalSemaphores{renderFinishedSemaphores[currentFrame].get()};

			inFlightFences[currentFrame].reset();

			context.commandSubmit_Graphics(
				swapChain.getCommandBuffers()[imageIndex],
				imageAvailableSemaphores[currentFrame],
				renderFinishedSemaphores[currentFrame],
				inFlightFences[currentFrame]
			);

			VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};

			presentInfo.waitSemaphoreCount = signalSemaphores.size();
			presentInfo.pWaitSemaphores = signalSemaphores.data();

			const std::array swapChains{VkSwapchainKHR(swapChain)};
			presentInfo.swapchainCount = swapChains.size();
			presentInfo.pSwapchains = swapChains.data();
			presentInfo.pImageIndices = &imageIndex;
			presentInfo.pResults = nullptr; // Optional

			swapChain.postImage(presentInfo);
		}


	private:
		void initVulkan(Window* window){
			context.init();

			swapChain.attachWindow(window);
			swapChain.createSurface(context.instance);

			context.selectPhysicalDevice(swapChain.getSurface());
			context.device = LogicalDevice{context.physicalDevice, context.physicalDevice.queues};

			swapChain.createSwapChain(context.physicalDevice, context.device);
			swapChain.presentQueue = context.device.getPresentQueue();

			swapChain.recreateCallback = [this](SwapChain&){
				createGraphicsPipeline();
				createCommandBuffers();
			};

			descriptorSetLayout = DescriptorSetLayout{context.device, [](DescriptorSetLayout& layout){
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
			}};

			descriptorPool = DescriptorSetPool{context.device, descriptorSetLayout.builder, swapChain.size()};

			createGraphicsPipeline();

			commandPool = CommandPool{context.device, context.physicalDevice.queues.graphicsFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT};

			vertexBuffer = createVertexBuffer(context.physicalDevice, context.device, commandPool, context.device.getGraphicsQueue());
			indexBuffer = createIndexBuffer(context.physicalDevice, context.device, commandPool, context.device.getGraphicsQueue());

			createTextureImage(context.physicalDevice, commandPool, context.device, context.device.getGraphicsQueue(), textureImage, textureImageMemory, mipLevels);
			textureImageView = createImageView(context.device, textureImage, swapChain.getFormat(), mipLevels);
			textureSampler = createTextureSampler(context.device, mipLevels);

			createUniformBuffer();
			createDescriptorSets();

			createCommandBuffers();
			createSyncObjects();

			std::cout.flush();
		}

		void createDescriptorSets() {
			descriptorSets = descriptorPool.obtain(swapChain.size(), descriptorSetLayout);

			for (std::size_t i = 0; i < swapChain.size(); i++) {
				auto bufferInfo = uniformBuffers[i].getDescriptorInfo();
				auto imageInfo = textureSampler.getDescriptorInfo_ShaderRead(textureImageView);

				DescriptorSetUpdator updator{context.device, descriptorSets[i]};

				updator.push(bufferInfo);
				updator.push(imageInfo);
				updator.update();
			}
		}

		void createUniformBuffer() {
			uniformBuffers.resize(swapChain.size());

			for (auto& buffer : uniformBuffers) {
				buffer = UniformBuffer{context.physicalDevice, context.device, sizeof(UniformBufferObject)};
			}
		}

		void createSyncObjects(){
			imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
			renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
			inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

			for(std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
				inFlightFences[i] = Fence{context.device, Fence::CreateFlags::signal};
				imageAvailableSemaphores[i] = Semaphore{context.device};
				renderFinishedSemaphores[i] = Semaphore{context.device};
			}
		}

		void createCommandBuffers(){

			for(const auto& [i, commandBuffer] : swapChain.getCommandBuffers() | std::views::enumerate){
				commandBuffer = commandPool.obtain();

				ScopedCommand scopedCommand{commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
				renderPassInfo.renderPass = swapChain.getRenderPass();
				renderPassInfo.framebuffer = swapChain.getFrameBuffers()[i];

				renderPassInfo.renderArea.offset = {0, 0};
				renderPassInfo.renderArea.extent = swapChain.getExtent();

				VkClearValue clearColor{};
				renderPassInfo.clearValueCount = 1;
				renderPassInfo.pClearValues = &clearColor;

				commandBuffer.push(vkCmdBeginRenderPass, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				commandBuffer.push(vkCmdBindPipeline, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

				commandBuffer.setViewport({
					.width = static_cast<float>(swapChain.getExtent().width),
					.height = static_cast<float>(swapChain.getExtent().height),
					.minDepth = 0.0f, .maxDepth = 1.f
				});

				commandBuffer.setScissor({ {}, swapChain.getExtent()});

				const VkBuffer vertexBuffers[]{vertexBuffer.get()};

				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, Default::NoOffset);

				vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

				vkCmdBindDescriptorSets(commandBuffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
                        1, descriptorSets[i].operator->(),
                        0, nullptr);


				vkCmdDrawIndexed(commandBuffer,
					static_cast<std::uint32_t>(test_indices.size()), 1, 0, 0, 0);


				vkCmdEndRenderPass(commandBuffer);
			}
		}

		void createGraphicsPipeline(){
			pipelineLayout = PipelineLayout{context.device, descriptorSetLayout.asSeq()};

			ShaderModule vertShaderModule{
				context.device, R"(D:\projects\vulkan_framework\properties\shader\spv\test.vert.spv)"
			};

			ShaderModule fragShaderModule{
				context.device, R"(D:\projects\vulkan_framework\properties\shader\spv\test.frag.spv)"
			};

			PipelineTemplate pipelineTemplate{};
			pipelineTemplate
				.useDefaultFixedStages()
				.setVertexInputInfo<BindInfo>()
				.setShaderChain({&vertShaderModule, &fragShaderModule})
				.setDynamicStates({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR})
				.setDynamicViewportCount(1);
			graphicsPipeline =
				GraphicPipeline{context.device, pipelineTemplate, pipelineLayout, swapChain.getRenderPass()};
		}
	};
}


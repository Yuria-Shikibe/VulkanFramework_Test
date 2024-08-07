module;

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
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
import Core.Vulkan.Buffer.ExclusiveBuffer;

import Core.Vulkan.Preinstall;

import Core.Vulkan.CommandPool;
import Core.Vulkan.Buffer.CommandBuffer;


export namespace Core::Vulkan{
	constexpr int MAX_FRAMES_IN_FLIGHT = 2;


	class VulkanManager{
	public:
		[[nodiscard]] explicit VulkanManager(Window* window){
			initVulkan(window);
		}

		~VulkanManager(){cleanup();}

		Context context{};

		SwapChain swapChain{};
		std::vector<VkFence> inFlightFences{};

		std::size_t currentFrame = 0;
		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;

		DescriptorSetLayout descriptorSetLayout{};
		DescriptorSetPool descriptorPool{};

		PipelineLayout pipelineLayout{};
		VkPipeline graphicsPipeline{};

		CommandPool commandPool{};
		std::vector<CommandBuffer> commandBuffers{};

		ExclusiveBuffer vertexBuffer{};
		ExclusiveBuffer indexBuffer{};
		std::vector<ExclusiveBuffer> uniformBuffers{};

		std::vector<VkDescriptorSet> descriptorSets{};

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


			auto data = uniformBuffers[currentImage].map();
			Geom::Matrix3D matrix3D{};
			matrix3D.setToRotation(time * 5);
			new (data) UniformBufferObject{matrix3D, 0};
			uniformBuffers[currentImage].unmap();
		}

		void drawFrame(){
			vkWaitForFences(context.device, 1, &inFlightFences[currentFrame], true, std::numeric_limits<uint64_t>::max());
			vkResetFences(context.device, 1, &inFlightFences[currentFrame]);

			std::uint32_t imageIndex{};
			VkResult result = vkAcquireNextImageKHR(context.device, swapChain,
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
			submitInfo.pCommandBuffers = &commandBuffers[imageIndex].get();

			const std::array signalSemaphores{renderFinishedSemaphores[currentFrame]};
			submitInfo.signalSemaphoreCount = signalSemaphores.size();
			submitInfo.pSignalSemaphores = signalSemaphores.data();

			vkResetFences(context.device, 1, &inFlightFences[currentFrame]);

			if (vkQueueSubmit(context.device.getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
				throw std::runtime_error("failed to submit draw command buffer!");
			}

			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

			presentInfo.waitSemaphoreCount = signalSemaphores.size();
			presentInfo.pWaitSemaphores = signalSemaphores.data();

			const std::array swapChains{VkSwapchainKHR(swapChain)};
			presentInfo.swapchainCount = swapChains.size();
			presentInfo.pSwapchains = swapChains.data();
			presentInfo.pImageIndices = &imageIndex;
			presentInfo.pResults = nullptr; // Optional


			result = vkQueuePresentKHR(context.device.getPresentQueue(), &presentInfo);

			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
				framebufferResized = false;
				recreateSwapChain();
			} else if (result != VK_SUCCESS) {
				throw std::runtime_error("failed to present swap chain image!");
			}

			currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		}


	private:
		void initVulkan(Window* window){
			context.init();

			swapChain.attachWindow(window);
			swapChain.createSurface(context.instance);

			context.selectPhysicalDevice(swapChain.getSurface());
			context.device = LogicalDevice{context.physicalDevice, context.physicalDevice.queues};

			swapChain.createSwapChain(context.physicalDevice, context.device);

			descriptorSetLayout = DescriptorSetLayout{context.device, [](DescriptorSetLayout& layout){
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
			}};

			descriptorPool = DescriptorSetPool{context.device, descriptorSetLayout.builder, swapChain.size()};

			createGraphicsPipeline();

			commandPool = CommandPool{context.device, context.physicalDevice.queues.graphicsFamily};

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
			std::vector<VkDescriptorSetLayout> layouts(swapChain.size(), descriptorSetLayout);
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = swapChain.size();
			allocInfo.pSetLayouts = layouts.data();

			descriptorSets.resize(swapChain.size());
			if (vkAllocateDescriptorSets(context.device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
				throw std::runtime_error("failed to allocate descriptor sets!");
			}

			for (std::size_t i = 0; i < swapChain.size(); i++) {
				VkDescriptorBufferInfo bufferInfo = {};
				bufferInfo.buffer = uniformBuffers[i];
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(UniformBufferObject);

				VkDescriptorImageInfo imageInfo = {};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = textureImageView;
				imageInfo.sampler = textureSampler;

				std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

				descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[0].dstSet = descriptorSets[i];
				descriptorWrites[0].dstBinding = 0;
				descriptorWrites[0].dstArrayElement = 0;
				descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrites[0].descriptorCount = 1;
				descriptorWrites[0].pBufferInfo = &bufferInfo;

				descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[1].dstSet = descriptorSets[i];
				descriptorWrites[1].dstBinding = 1;
				descriptorWrites[1].dstArrayElement = 0;
				descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[1].descriptorCount = 1;
				descriptorWrites[1].pImageInfo = &imageInfo;

				vkUpdateDescriptorSets(context.device, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);

			}


			for (size_t i = 0; i < swapChain.size(); i++) {
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

				vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);
			}
		}

		void createUniformBuffer() {
			VkDeviceSize bufferSize = sizeof(UniformBufferObject);

			uniformBuffers.resize(swapChain.size());

			for (auto& buffer : uniformBuffers) {

				buffer = ExclusiveBuffer{
					context.physicalDevice, context.device, bufferSize,
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
				};
			}
		}

		void recreateSwapChain() {
			while (swapChain.getTargetWindow()->getSize().area() == 0) {
				swapChain.getTargetWindow()->waitEvent();
			}

			vkDeviceWaitIdle(context.device);

			cleanupSwapChain();

			swapChain.createSwapChain(context.physicalDevice, context.device);

			createGraphicsPipeline();
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
				if(vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
					vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
					vkCreateFence(context.device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS){
					throw std::runtime_error("failed to create synchronization objects for a frame!");
				}
			}
		}

		void createCommandBuffers(){
			commandBuffers = commandPool.obtainArray(swapChain.size());

			for(const auto& [i, commandBuffer] : commandBuffers | std::views::enumerate){
				ScopedCommand scopedCommand{commandBuffer};

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

				commandBuffer.setScissor({ {}, swapChain.getExtent(),});

				const VkBuffer vertexBuffers[]{vertexBuffer.getHandler()};

				const VkDeviceSize offsets[]{0};

				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

				vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

				vkCmdBindDescriptorSets(commandBuffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0,
                        nullptr);


				vkCmdDrawIndexed(commandBuffer,
					static_cast<std::uint32_t>(test_indices.size()), 1, 0, 0, 0);


				vkCmdEndRenderPass(commandBuffer);
			}
		}

		void createGraphicsPipeline(){
			pipelineLayout = PipelineLayout{context.device, std::array{static_cast<VkDescriptorSetLayout>(descriptorSetLayout)}};

			ShaderModule vertShaderModule{
				context.device, R"(D:\projects\vulkan_framework\properties\shader\spv\test.vert.spv)"
			};

			ShaderModule fragShaderModule{
				context.device, R"(D:\projects\vulkan_framework\properties\shader\spv\test.frag.spv)"
			};


			ShaderChain shaderChain{&vertShaderModule, &fragShaderModule};

			const auto vertexInputInfo = BindInfo::createInfo();

			std::vector dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

			const auto dynamicState = Util::createDynamicState(dynamicStates);

			VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
			pipelineInfo.stageCount = shaderChain.size();
			pipelineInfo.pStages = shaderChain.data();
			pipelineInfo.pVertexInputState = &vertexInputInfo;
			pipelineInfo.pInputAssemblyState = &Default::InputAssembly;
			pipelineInfo.pViewportState = &Default::DynamicViewportState<1>;
			pipelineInfo.pRasterizationState = &Default::Rasterizer;
			pipelineInfo.pMultisampleState = &Default::Multisampling;
			pipelineInfo.pColorBlendState = &Default::ColorBlending;
			pipelineInfo.pDynamicState = &dynamicState;
			pipelineInfo.layout = pipelineLayout;
			pipelineInfo.renderPass = swapChain.getRenderPass();
			pipelineInfo.subpass = 0;
			pipelineInfo.basePipelineHandle = nullptr;

			if(vkCreateGraphicsPipelines(context.device, nullptr, 1, &pipelineInfo, nullptr, &graphicsPipeline) !=
				VK_SUCCESS){
				throw std::runtime_error("failed to create graphics pipeline!");
			}
		}

		void cleanupSwapChain() {
			vkDestroyPipeline(context.device, graphicsPipeline, nullptr);

			pipelineLayout = {};

			commandBuffers.clear();

			swapChain.cleanupSwapChain();
		}

		void cleanup(){
			vkDestroyPipeline(context.device, graphicsPipeline, nullptr);


			vkDestroyImageView(context.device, textureImageView, nullptr);
			vkDestroyImage(context.device, textureImage, nullptr);
			vkFreeMemory(context.device, textureImageMemory, nullptr);

			for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				vkDestroySemaphore(context.device, renderFinishedSemaphores[i], nullptr);
				vkDestroySemaphore(context.device, imageAvailableSemaphores[i], nullptr);
				vkDestroyFence(context.device, inFlightFences[i], nullptr);
			}
		}

	};
}


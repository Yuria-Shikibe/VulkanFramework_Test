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

import Core.Vulkan.Context;
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
import Core.Vulkan.Buffer.IndexBuffer;
import Core.Vulkan.Buffer.VertexBuffer;
import Core.Vulkan.Sampler;

import OS.File;


import Assets.Graphic;

export namespace Core::Vulkan{
	class VulkanManager{
	public:
		[[nodiscard]] VulkanManager() = default;

		~VulkanManager(){
			vkDestroyRenderPass(context.device, renderPass, nullptr);
		}

		Context context{};

		CommandPool commandPool{};
		CommandPool transientCommandPool{};

		DescriptorSetPool descriptorPool{};

		SwapChain swapChain{};
		VkRenderPass renderPass{};

		struct InFlightData{
			std::atomic_bool alreadyCleared{false};

			Fence inFlightFence{};
			Semaphore imageAvailableSemaphore{};
			Semaphore renderFinishedSemaphore{};
			Semaphore clearSemaphore{};
			// Semaphore vertexFlushSemaphore{};

			CommandBuffer vertexFlushCommandBuffer{};
		};

		std::array<InFlightData, MAX_FRAMES_IN_FLIGHT> frameDataArr{};

		// DescriptorSetLayout descriptorSetLayout{};

		PipelineLayout pipelineLayout{};
		GraphicPipeline graphicsPipeline{};

		// ExclusiveBuffer vertexBuffer{};
		// IndexBuffer indexBuffer{};
		// DynamicVertexBuffer<Vertex> dyVertexBuffer{};
		// PersistentTransferDoubleBuffer instanceBuffer{};
		// UniformBuffer uniformBuffer{};
		// DescriptorSet descriptorSet{};

		Texture texture1{};
		Texture texture2{};
		Texture texture3{};

		struct UsedPipelineResources{
			VkBuffer vertexBuffer{};
			VkBuffer indexBuffer{};
			VkIndexType indexType{};
			VkBuffer indirectBuffer{};

			VkDescriptorSetLayout descriptorSetLayout{};
			VkDescriptorSet descriptorSet{};
		} usedPipelineResources{};

		void drawBegin(){
			auto currentFrame = swapChain.getCurrentInFlightImage();
			auto& currentFrameData = frameDataArr[currentFrame];

			currentFrameData.inFlightFence.waitAndReset();
			const auto imageIndex = swapChain.acquireNextImage(currentFrameData.imageAvailableSemaphore);

			context.commandSubmit_Graphics(VkSubmitInfo{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = 1u,
				.pWaitSemaphores = currentFrameData.imageAvailableSemaphore.asData(),
				.pWaitDstStageMask = Seq::StageFlagBits<VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT>,

				.commandBufferCount = 1u,
				.pCommandBuffers = swapChain.getCommandClears()[imageIndex].asData(),

				.signalSemaphoreCount = 1,
				.pSignalSemaphores = currentFrameData.clearSemaphore.asData(),
			}, currentFrameData.inFlightFence);
		}

		void drawFrame(VkSemaphore toWait, VkSemaphore toSignal, bool isLast){
			auto currentFrame = swapChain.getCurrentInFlightImage();
			auto& currentFrameData = frameDataArr[currentFrame];
			auto imageIndex = swapChain.getCurrentImageIndex();

			const std::array semaphoresToWait{
				toWait,
				currentFrameData.clearSemaphore.get()
			};

			const bool isCleared = currentFrameData.alreadyCleared.exchange(true);

			VkSubmitInfo submitInfo{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = static_cast<std::uint32_t>(semaphoresToWait.size()) - isCleared,
				.pWaitSemaphores = semaphoresToWait.data(),
				.pWaitDstStageMask =
					Seq::StageFlagBits<
						VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
				>,
				.commandBufferCount = 1u,
				.pCommandBuffers = swapChain.getCommandPresents()[imageIndex].asData(),
			};

			if(isLast){
				const std::array semaphoresToSignal{toSignal, currentFrameData.renderFinishedSemaphore.get()};

				// currentFrameData.inFlightFence.reset();
				submitInfo.signalSemaphoreCount = semaphoresToSignal.size();
				submitInfo.pSignalSemaphores = semaphoresToSignal.data();

				context.commandSubmit_Graphics(submitInfo, nullptr);
			}else{
				const std::array semaphoresToSignal{toSignal};

				submitInfo.signalSemaphoreCount = semaphoresToSignal.size();
				submitInfo.pSignalSemaphores = semaphoresToSignal.data();

				context.commandSubmit_Graphics(submitInfo, nullptr);
			}
		}

		void drawEnd(){
			auto currentFrame = swapChain.getCurrentInFlightImage();
			auto& currentFrameData = frameDataArr[currentFrame];
			auto imageIndex = swapChain.getCurrentImageIndex();

			VkPresentInfoKHR presentInfo{};

			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = currentFrameData.renderFinishedSemaphore.asData();
			presentInfo.pImageIndices = &imageIndex;

			swapChain.postImage(presentInfo);

			currentFrameData.alreadyCleared = false;
		}

		void preInitVulkan(Window* window){
			context.init();

			swapChain.attachWindow(window);
			swapChain.createSurface(context.instance);

			context.selectPhysicalDevice(swapChain.getSurface());
			context.device = LogicalDevice{context.physicalDevice, context.physicalDevice.queues};


			swapChain.createSwapChain(context.physicalDevice, context.device);
			swapChain.presentQueue = context.device.getPresentQueue();

			createRenderPass();
			swapChain.setRenderPass(renderPass);
			swapChain.createSwapChainFramebuffers();

			swapChain.recreateCallback = [this](SwapChain&){
				createGraphicsPipeline();
				createCommandBuffers();
				createClearCommands();
			};

			// descriptorPool = DescriptorSetPool{context.device, descriptorSetLayout.builder, swapChain.size()};

			commandPool = CommandPool{context.device, context.physicalDevice.queues.graphicsFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT};
			transientCommandPool = CommandPool{context.device, context.physicalDevice.queues.graphicsFamily, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT};

		}

		void setPipelineLayout(){
			std::array seq{usedPipelineResources.descriptorSetLayout};
			pipelineLayout = PipelineLayout{context.device, seq};
		}

		void postInitVulkan(){
			setPipelineLayout();
			//Context Ready
			// descriptorSetLayout = DescriptorSetLayout{context.device, [](DescriptorSetLayout& layout){
			// 	layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
			// 	layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3);
			// }};
			createGraphicsPipeline();

			// vertexBuffer = createVertexBuffer(context, commandPool);
			// indexBuffer = Util::createIndexBuffer(context, transientCommandPool, Util::BatchIndices<>);
			//
			// dyVertexBuffer = DynamicVertexBuffer<Vertex>{context.physicalDevice, context.device, Util::BatchIndices<>.size() / Util::PrimitiveVertCount};

			texture1 = Texture{
				context.physicalDevice, context.device,
				R"(D:\projects\vulkan_framework\properties\texture\src.png)",
				transientCommandPool.obtainTransient(context.device.getGraphicsQueue())
			};

			texture2 = Texture{
				context.physicalDevice, context.device,
				R"(D:\projects\vulkan_framework\properties\texture\csw.png)",
				transientCommandPool.obtainTransient(context.device.getGraphicsQueue())
			};

			texture3 = Texture{
				context.physicalDevice, context.device,
				R"(D:\projects\vulkan_framework\properties\texture\yyz.png)",
				transientCommandPool.obtainTransient(context.device.getGraphicsQueue())
			};

			// uniformBuffer = UniformBuffer{context.physicalDevice, context.device, sizeof(UniformBlock)};
			// createDescriptorSets();

			createCommandBuffers();
			createClearCommands();

			createFrameObjects();

			std::cout.flush();
		}

	private:


		void createFrameObjects(){
			for (auto& frameData : frameDataArr){
				frameData.inFlightFence = Fence{context.device, Fence::CreateFlags::signal};
				frameData.imageAvailableSemaphore = Semaphore{context.device};
				frameData.renderFinishedSemaphore = Semaphore{context.device};
				frameData.clearSemaphore = Semaphore{context.device};
				// frameData.vertexFlushSemaphore = Semaphore{context.device};
				frameData.vertexFlushCommandBuffer = commandPool.obtain();
			}
		}
	public:

		void createClearCommands(){
			for(const auto& [i, commandBuffer] : swapChain.getCommandClears() | std::views::enumerate){
				commandBuffer = commandPool.obtain();

				ScopedCommand scopedCommand{commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
				renderPassInfo.renderPass = renderPass;
				renderPassInfo.framebuffer = swapChain.getFrameBuffers()[i];

				renderPassInfo.renderArea.offset = {0, 0};
				renderPassInfo.renderArea.extent = swapChain.getExtent();

				VkClearValue clearColor{};
				renderPassInfo.clearValueCount = 1;
				renderPassInfo.pClearValues = &clearColor;

				vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				VkClearAttachment clearAttachment = {};
				clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				clearAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

				VkClearRect clearRect = {};
				clearRect.rect.offset = {0, 0};
				clearRect.rect.extent = swapChain.getExtent();
				clearRect.baseArrayLayer = 0;
				clearRect.layerCount = 1;

				vkCmdClearAttachments(commandBuffer, 1, &clearAttachment, 1, &clearRect);

				vkCmdEndRenderPass(commandBuffer);
			}
		}

		void createCommandBuffers(){
			for(const auto& [i, commandBuffer] : swapChain.getCommandPresents() | std::views::enumerate){
				commandBuffer = commandPool.obtain();

				ScopedCommand scopedCommand{commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
				renderPassInfo.renderPass = renderPass;
				renderPassInfo.framebuffer = swapChain.getFrameBuffers()[i];

				renderPassInfo.renderArea.offset = {0, 0};
				renderPassInfo.renderArea.extent = swapChain.getExtent();

				VkClearValue clearColor{};
				renderPassInfo.clearValueCount = 1;
				renderPassInfo.pClearValues = &clearColor;

				vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				commandBuffer.push(vkCmdBindPipeline, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);


				vkCmdBindVertexBuffers(commandBuffer, 0, 1,
					&usedPipelineResources.vertexBuffer, Seq::NoOffset);


				vkCmdBindIndexBuffer(commandBuffer, usedPipelineResources.indexBuffer, 0, usedPipelineResources.indexType);

				::vkCmdBindDescriptorSets(commandBuffer,
									VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
									1, &usedPipelineResources.descriptorSet,
									0, nullptr);

				vkCmdDrawIndexedIndirect(commandBuffer, usedPipelineResources.indirectBuffer, 0, 1, sizeof(VkDrawIndexedIndirectCommand));
				// vkCmdDrawIndexedIndirectCount(commandBuffer, dyVertexBuffer.getIndirectCmdBuffer().get(), 0, 1, sizeof(VkDrawIndexedIndirectCommand), );

				// VkDrawIndirectCount

				vkCmdEndRenderPass(commandBuffer);
			}

		}

		void createGraphicsPipeline(){
			auto extent = swapChain.getExtent();

			PipelineTemplate pipelineTemplate{};
			pipelineTemplate
				.useDefaultFixedStages()
				.setVertexInputInfo<VertBindInfo>()
				.setShaderChain({&Assets::Shader::batchVertShader, &Assets::Shader::batchFragShader})
				.setStaticScissors({{}, extent})
				.setStaticViewport({
					0, 0,
					static_cast<float>(extent.width), static_cast<float>(extent.height),
					0.f, 1.f});
			graphicsPipeline =
				GraphicPipeline{context.device, pipelineTemplate, pipelineLayout, renderPass};
		}


		void createRenderPass(){
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = swapChain.getFormat();
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //LOAD;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //TODO
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

			if(vkCreateRenderPass(context.device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS){
				throw std::runtime_error("failed to create render pass!");
			}
		}
	};
}


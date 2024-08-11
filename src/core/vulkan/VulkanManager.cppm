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


export namespace Core::Vulkan{
	class VulkanManager{
	public:
		[[nodiscard]] explicit VulkanManager(Window* window){
			initVulkan(window);
		}


		Context context{};

		CommandPool commandPool{};
		DescriptorSetPool descriptorPool{};

		SwapChain swapChain{};

		struct InFlightData{
			Fence inFlightFence{};
			Semaphore imageAvailableSemaphore{};
			Semaphore renderFinishedSemaphore{};
			Semaphore vertexFlushSemaphore{};

			CommandBuffer vertexFlushCommandBuffer{};
		};

		std::array<InFlightData, MAX_FRAMES_IN_FLIGHT> frameDataArr{};

		DescriptorSetLayout descriptorSetLayout{};

		PipelineLayout pipelineLayout{};
		GraphicPipeline graphicsPipeline{};

		// ExclusiveBuffer vertexBuffer{};
		IndexBuffer indexBuffer{};
		DynamicVertexBuffer<Vertex> dyVertexBuffer{};

		std::vector<UniformBuffer> uniformBuffers{};

		std::vector<DescriptorSet> descriptorSets{};

		std::uint32_t mipLevels{};

		Image textureImage{};
		ImageView textureImageView{};

		Sampler textureSampler{};

		void updateUniformBuffer(const std::uint32_t currentImage) const{
			auto data = uniformBuffers[currentImage].memory.map();
			Geom::Matrix3D matrix3D{};
			matrix3D.setOrthogonal(swapChain.getTargetWindow()->getSize().as<float>());
			new (data) UniformBufferObject{matrix3D, 0.1f};

			uniformBuffers[currentImage].memory.unmap();
		}

		void submitVertexPost(const InFlightData& currentFrameData) const{
			auto& frameCmd = currentFrameData.vertexFlushCommandBuffer;

			frameCmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			dyVertexBuffer.cmdFlushToDevice(frameCmd);

			frameCmd.end();

			context.commandSubmit_Graphics(frameCmd,
				nullptr,
				currentFrameData.vertexFlushSemaphore,
				nullptr,
				0
			);
		}

		void drawFrame(){
			if (auto [p, failed] = dyVertexBuffer.acquireSegment(); !failed){
				std::memcpy(p, test_vertices.data(), decltype(dyVertexBuffer)::UnitOffset);
			}


			auto currentFrame = swapChain.getCurrentRenderingImage();
			auto& currentFrameData = frameDataArr[currentFrame];

			dyVertexBuffer.flush();

			currentFrameData.inFlightFence.waitAndReset();

			submitVertexPost(currentFrameData);

			const auto imageIndex = swapChain.acquireNextImage(currentFrameData.imageAvailableSemaphore);

			updateUniformBuffer(imageIndex);


			const std::array semaphoresToWait{currentFrameData.imageAvailableSemaphore.get(), currentFrameData.vertexFlushSemaphore.get()};

			// currentFrameData.inFlightFence.reset();

			context.commandSubmit_Graphics(VkSubmitInfo{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = static_cast<std::uint32_t>(semaphoresToWait.size()),
				.pWaitSemaphores = semaphoresToWait.data(),
				.pWaitDstStageMask = Seq::StageFlagBits<VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT>,
				.commandBufferCount = 1u,
				.pCommandBuffers = swapChain.getCommandBuffers()[imageIndex].asData(),
				.signalSemaphoreCount = 1u,
				.pSignalSemaphores = currentFrameData.renderFinishedSemaphore.asData(),
			}, currentFrameData.inFlightFence);


			VkPresentInfoKHR presentInfo{};
			const auto signalSemaphores = currentFrameData.renderFinishedSemaphore.asSeq();

			presentInfo.waitSemaphoreCount = signalSemaphores.size();
			presentInfo.pWaitSemaphores = signalSemaphores.data();

			presentInfo.pImageIndices = &imageIndex;
			presentInfo.pResults = nullptr; // Optional

			swapChain.postImage(presentInfo);

			dyVertexBuffer.reset();
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

			// vertexBuffer = createVertexBuffer(context, commandPool);
			indexBuffer = Util::createIndexBuffer(context, commandPool, Util::BatchIndices);

			dyVertexBuffer = DynamicVertexBuffer<Vertex>{context.physicalDevice, context.device, Util::BatchIndices.size() / Util::PrimitiveVertCount};
			dyVertexBuffer.map();

			createTextureImage(context.physicalDevice, commandPool, context.device, context.device.getGraphicsQueue(), textureImage, mipLevels);
			textureImageView = ImageView(context.device, textureImage, VK_FORMAT_R8G8B8A8_UNORM, mipLevels);
			textureSampler = createTextureSampler(context.device, mipLevels);

			createUniformBuffer();
			createDescriptorSets();

			createCommandBuffers();

			createFrameObjects();

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

		void createFrameObjects(){
			for (auto& frameData : frameDataArr){
				frameData.inFlightFence = Fence{context.device, Fence::CreateFlags::signal};
				frameData.imageAvailableSemaphore = Semaphore{context.device};
				frameData.renderFinishedSemaphore = Semaphore{context.device};
				frameData.vertexFlushSemaphore = Semaphore{context.device};
				frameData.vertexFlushCommandBuffer = commandPool.obtain();
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

				const VkBuffer vertexBuffers[]{dyVertexBuffer.getVertexBuffer().get()};

				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, Seq::NoOffset);


				vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, indexBuffer.indexType);

				vkCmdBindDescriptorSets(commandBuffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
                        1, descriptorSets[i].operator->(),
                        0, nullptr);

				vkCmdDrawIndexedIndirect(commandBuffer, dyVertexBuffer.getIndirectCmdBuffer().get(), 0, 1, sizeof(VkDrawIndexedIndirectCommand));

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


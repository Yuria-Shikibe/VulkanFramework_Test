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

		Context context{};

		CommandPool commandPool{};
		CommandPool transientCommandPool{};

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

		Texture texture1{};
		Texture texture2{};
		Texture texture3{};

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

			swapChain.postImage(presentInfo);

			dyVertexBuffer.reset();
		}


		void preInitVulkan(Window* window){
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

			descriptorPool = DescriptorSetPool{context.device, descriptorSetLayout.builder, swapChain.size()};

			commandPool = CommandPool{context.device, context.physicalDevice.queues.graphicsFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT};
			transientCommandPool = CommandPool{context.device, context.physicalDevice.queues.graphicsFamily, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT};
		}

		void postInitVulkan(){
			//Context Ready
			descriptorSetLayout = DescriptorSetLayout{context.device, [](DescriptorSetLayout& layout){
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3);
			}};
			createGraphicsPipeline();


			// vertexBuffer = createVertexBuffer(context, commandPool);
			indexBuffer = Util::createIndexBuffer(context, transientCommandPool, Util::BatchIndices);

			dyVertexBuffer = DynamicVertexBuffer<Vertex>{context.physicalDevice, context.device, Util::BatchIndices.size() / Util::PrimitiveVertCount};

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


			textureSampler = Sampler(context.device, Samplers::TextureSampler);

			createUniformBuffer();
			createDescriptorSets();

			createCommandBuffers();

			createFrameObjects();

			std::cout.flush();
		}

	private:
		void createDescriptorSets() {
			descriptorSets = descriptorPool.obtain(swapChain.size(), descriptorSetLayout);

			for (std::size_t i = 0; i < swapChain.size(); i++) {
				auto bufferInfo = uniformBuffers[i].getDescriptorInfo();

				auto imageInfo =
					textureSampler.getDescriptorInfo_ShaderRead(texture1.getView(), texture2.getView(), texture3.getView());

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


				vkCmdBindVertexBuffers(commandBuffer, 0, 1,
					dyVertexBuffer.vertexBuffer.getTargetBuffer().asData(), Seq::NoOffset);


				vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, indexBuffer.indexType);

				vkCmdBindDescriptorSets(commandBuffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
                        1, descriptorSets[i].asData(),
                        0, nullptr);

				vkCmdDrawIndexedIndirect(commandBuffer, dyVertexBuffer.indirectBuffer.getTargetBuffer().get(), 0, 1, sizeof(VkDrawIndexedIndirectCommand));
				// vkCmdDrawIndexedIndirectCount(commandBuffer, dyVertexBuffer.getIndirectCmdBuffer().get(), 0, 1, sizeof(VkDrawIndexedIndirectCommand), );

				// VkDrawIndirectCount

				vkCmdEndRenderPass(commandBuffer);
			}
		}

		void createGraphicsPipeline(){
			pipelineLayout = PipelineLayout{context.device, descriptorSetLayout.asSeq()};

			auto extent = swapChain.getExtent();

			auto [vinfo, vdata] = Util::getVertexGroupInfo<VertBindInfo, InstanceBindInfo>();


			PipelineTemplate pipelineTemplate{};
			pipelineTemplate
				.useDefaultFixedStages()
				.setVertexInputInfo(vinfo)
				.setShaderChain({&Assets::Shader::batchVertShader, &Assets::Shader::batchFragShader})
				.setStaticScissors({{}, extent})
				.setStaticViewport({
					0, 0,
					static_cast<float>(extent.width), static_cast<float>(extent.height),
					0.f, 1.f});
			graphicsPipeline =
				GraphicPipeline{context.device, pipelineTemplate, pipelineLayout, swapChain.getRenderPass()};
		}
	};
}


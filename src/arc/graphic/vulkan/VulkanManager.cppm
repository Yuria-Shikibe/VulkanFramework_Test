module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Manager;

import std;

import ext.MetaProgramming;

import Core.Window;
import Core.Vulkan.Uniform;
import Core.Vulkan.Vertex;

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
import Core.Vulkan.RenderPass;

import Core.Vulkan.CommandPool;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Buffer.IndexBuffer;
import Core.Vulkan.Buffer.VertexBuffer;
import Core.Vulkan.Buffer.FrameBuffer;
import Core.Vulkan.Sampler;
import Core.Vulkan.Image;
import Core.Vulkan.Comp;
import Core.Vulkan.RenderPassGroup;
import Core.Vulkan.Attachment;
import Core.Vulkan.BatchData;

import Graphic.PostProcessor;

import OS.File;

import Assets.Graphic;
import Assets.Graphic.PostProcess;

export namespace Core::Vulkan{
	class VulkanManager{
	public:
		[[nodiscard]] VulkanManager() = default;

		Context context{};
		CommandPool commandPool{};
		CommandPool transientCommandPool{};


		SwapChain swapChain{};

		RenderProcedure batchDrawPass{};
		RenderProcedure flushPass{};

		[[nodiscard]] TransientCommand obtainTransientCommand() const{
			return transientCommandPool.obtainTransient(context.device.getGraphicsQueue());
		}

		FramebufferLocal batchFrameBuffer{};

		FramebufferLocal testFrameBuffer{};
		ColorAttachment finalStagingAttachment{};

		std::atomic_bool descriptorSetUpdated{false};
		Fence batchFence{};
		CommandBuffer batchDrawCommand{};

		Graphic::PostProcessor postProcess{};

		struct InFlightData{
			Fence inFlightFence{};
			Semaphore imageAvailableSemaphore{};
			Semaphore renderFinishedSemaphore{};
			Semaphore flushFinishedSemaphore{};

		};

		std::array<InFlightData, MAX_FRAMES_IN_FLIGHT> frameDataArr{};

		BatchData batchVertexData{};

		void drawFrame(bool isLast, const Fence& fence){
			auto currentFrame = swapChain.getCurrentInFlightImage();
			auto& currentFrameData = frameDataArr[currentFrame];

			VkSubmitInfo submitInfo{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.commandBufferCount = 1u,
					.pCommandBuffers = batchDrawCommand.asData(),
				};

			if(descriptorSetUpdated) [[unlikely]] {
				descriptorSetUpdated = false;
			}else{
				batchFence.waitAndReset();
			}

			fence.wait();

			if(isLast){
				const std::array semaphoresToSignal{currentFrameData.renderFinishedSemaphore.get()};

				submitInfo.signalSemaphoreCount = semaphoresToSignal.size();
				submitInfo.pSignalSemaphores = semaphoresToSignal.data();

				context.commandSubmit_Graphics(submitInfo, batchFence);
			} else{
				context.commandSubmit_Graphics(submitInfo, batchFence);
			}
		}

		void blitToScreen(){
			const auto currentFrame = swapChain.getCurrentInFlightImage();
			const auto& currentFrameData = frameDataArr[currentFrame];

			postProcess.submitCommand(currentFrameData.renderFinishedSemaphore);

			currentFrameData.inFlightFence.waitAndReset();
			const auto imageIndex = swapChain.acquireNextImage(currentFrameData.imageAvailableSemaphore);

			const std::array toWait{
					currentFrameData.imageAvailableSemaphore.get(), postProcess.getToWait()
				};

			context.commandSubmit_Graphics(
				VkSubmitInfo{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.waitSemaphoreCount = static_cast<std::uint32_t>(toWait.size()),
					.pWaitSemaphores = toWait.data(),
					.pWaitDstStageMask = Seq::StageFlagBits<
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT>,

					.commandBufferCount = 1u,
					.pCommandBuffers = swapChain.getCommandFlushes()[imageIndex].asData(),

					.signalSemaphoreCount = 1,
					.pSignalSemaphores = currentFrameData.flushFinishedSemaphore.asData(),
				}, currentFrameData.inFlightFence);

			VkPresentInfoKHR presentInfo{};

			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = currentFrameData.flushFinishedSemaphore.asData();
			presentInfo.pImageIndices = &imageIndex;

			swapChain.postImage(presentInfo);
		}

		void initContext(Window* window){
			context.init();

			swapChain.attachWindow(window);
			swapChain.createSurface(context.instance);

			context.createDevice(swapChain.getSurface());

			swapChain.createSwapChain(context.physicalDevice, context.device);
			swapChain.presentQueue = context.device.getPresentQueue();

			commandPool = CommandPool{
					context.device, context.physicalDevice.queues.graphicsFamily,
					VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
				};
			transientCommandPool = CommandPool{
					context.device, context.physicalDevice.queues.graphicsFamily,
				VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
				};

			batchFence = Fence{context.device, Fence::CreateFlags::signal};

			batchDrawCommand = commandPool.obtain();
			for (auto && commandBuffer : swapChain.getCommandFlushes()){
				commandBuffer = commandPool.obtain();
			}
		}

		void initVulkan(){
			setFlushGroup();
			setBatchGroup();

			createFrameBuffers();
			updateInputDescriptorSet();

			swapChain.recreateCallback = [this](SwapChain& swapChain){
				resize(swapChain);//createFrameBuffers();

				finalStagingAttachment.resize(swapChain.size2D(), obtainTransientCommand());
				flushPass.resize(swapChain.size2D());
				batchDrawPass.resize(swapChain.size2D());

				updateInputDescriptorSet();

				createDrawCommands();
				createFlushCommands();
			};

			createPostEffectPipeline();

			createFrameObjects();
			createFlushCommands();

			std::cout.flush();
		}

		void updateDescriptorSet(std::span<VkDescriptorImageInfo> imageInfo){
			DescriptorSetUpdator updator{context.device, batchDrawPass.front().descriptorSets[0]};

			auto bufferInfo = batchDrawPass.front().uniformBuffers[0].getDescriptorInfo();

			updator.push(bufferInfo);
			if(!imageInfo.empty())updator.push(imageInfo);

			if(!imageInfo.empty()){
				batchFence.waitAndReset();
				descriptorSetUpdated = true;
			}

			updator.update();
		}

	private:
		void createFrameObjects(){
			for(auto& frameData : frameDataArr){
				frameData.inFlightFence = Fence{context.device, Fence::CreateFlags::signal};
				frameData.imageAvailableSemaphore = Semaphore{context.device};
				frameData.renderFinishedSemaphore = Semaphore{context.device};
				frameData.flushFinishedSemaphore = Semaphore{context.device};
			}
		}

		void createFrameBuffers(){
			auto cmd = obtainTransientCommand();
			batchFrameBuffer = FramebufferLocal{context.device, swapChain.getTargetWindow()->getSize(), batchDrawPass.renderPass};

			ColorAttachment colorAttachment{context.physicalDevice, context.device};
			colorAttachment.create(
				swapChain.getTargetWindow()->getSize(), cmd,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			);

			batchFrameBuffer.pushCapturedAttachments(std::move(colorAttachment));

			testFrameBuffer = FramebufferLocal{context.device, swapChain.getTargetWindow()->getSize(), batchDrawPass.renderPass};

			finalStagingAttachment = ColorAttachment{context.physicalDevice, context.device};
			finalStagingAttachment.create(
					swapChain.getTargetWindow()->getSize(), cmd,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
				);

			cmd = {};

			for(const auto& [i, group] : swapChain.getImageData() | std::ranges::views::enumerate){
				group.framebuffer = FramebufferLocal{context.device, swapChain.getTargetWindow()->getSize(), flushPass.renderPass};

				group.framebuffer.loadExternalImageView({
					{0, group.imageView.get()},
					{1, finalStagingAttachment.getView().get()}
				});
				group.framebuffer.create();
			}

			batchFrameBuffer.loadCapturedAttachments(1);
			batchFrameBuffer.create();
		}

		void resize(SwapChain& swapChain){
			auto cmd = obtainTransientCommand();
			batchFrameBuffer.resize(swapChain.size2D(), cmd);

			for(const auto& [i, group] : swapChain.getImageData() | std::views::enumerate){
				group.framebuffer.resize(swapChain.size2D(), cmd, {{0, group.imageView.get()}, {1, batchFrameBuffer.localAttachments[0].getView().get()}});
			}
		}

	public:

		void setFlushGroup(){
			flushPass.init(context);

			flushPass.createRenderPass([this](RenderPass& renderPass){
				// Surface attachment
				renderPass.pushAttachment(VkAttachmentDescription{
						.format = swapChain.getFormat(),
						.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
						.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
						.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
					} | AttachmentDesc::Default |= AttachmentDesc::Stencil_DontCare);

				renderPass.pushAttachment(VkAttachmentDescription{
						.format = VK_FORMAT_R8G8B8A8_UNORM,
						.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
						.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
					} | AttachmentDesc::Default |= AttachmentDesc::Stencil_DontCare);

				renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
					subpass.addDependency(VkSubpassDependency{
							.srcSubpass = subpass.index,
							.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
							.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
							.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT | VK_DEPENDENCY_VIEW_LOCAL_BIT
						});

					subpass.addAttachment(
						RenderPass::AttachmentReference::Category::color, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
					subpass.addAttachment(
						RenderPass::AttachmentReference::Category::input, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
				});
			});

			flushPass.pushAndInitPipeline([this](RenderProcedure::PipelineData& pipeline){
				pipeline.createDescriptorLayout([this](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
				});

				pipeline.createPipelineLayout();

				pipeline.createDescriptorSet(swapChain.size());

				pipeline.builder = [](RenderProcedure::PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.useDefaultFixedStages()
						.setShaderChain({&Assets::Shader::blitSingleVert, &Assets::Shader::blitSingleFrag})
						.setStaticViewportAndScissor(pipeline.size().x, pipeline.size().y);

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			flushPass.resize(swapChain.size2D());
		}

		void setBatchGroup(){
			batchDrawPass.init(context);

			batchDrawPass.createRenderPass([](RenderPass& renderPass){
				//Drawing attachment
				renderPass.pushAttachment(VkAttachmentDescription{
						.format = VK_FORMAT_R8G8B8A8_UNORM,
						.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
						.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
						.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
					} | AttachmentDesc::Default |= AttachmentDesc::Stencil_DontCare);


				renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
					subpass.addDependency(VkSubpassDependency{
							.dstSubpass = 0,
							.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
						} | Subpass::Dependency::External);

					subpass.addAttachment(
						RenderPass::AttachmentReference::Category::color, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				});
			});

			batchDrawPass.pushAndInitPipeline([](RenderProcedure::PipelineData& pipeline){
				pipeline.createDescriptorLayout([](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (8));
				});

				pipeline.createPipelineLayout();

				pipeline.createDescriptorSet(1);
				pipeline.addUniformBuffer(sizeof(UniformBlock));

				pipeline.builder = [](RenderProcedure::PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.useDefaultFixedStages()
						.setVertexInputInfo<VertBindInfo>()
						.setShaderChain({&Assets::Shader::batchVertShader, &Assets::Shader::batchFragShader})
						.setStaticScissors({{}, std::bit_cast<VkExtent2D>(pipeline.size())})
						.setStaticViewport(float(pipeline.size().x), float(pipeline.size().y));

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			batchDrawPass.resize(swapChain.size2D());
		}

		void createPostEffectPipeline(){
			Graphic::AttachmentPort port{};
			port.in.insert_or_assign(0u, batchFrameBuffer.atLocal(0).getView());
			port.out.insert_or_assign(3u, finalStagingAttachment.getView());

			postProcess = Assets::PostProcess::Factory::blurProcessorFactory.generate(std::move(port), swapChain.size2D());
		}

		void updateInputDescriptorSet(){
			for(std::size_t i = 0; i < swapChain.size(); ++i){
				DescriptorSetUpdator descriptorSetUpdator{context.device, flushPass.pipelinesLocal[0].descriptorSets[i]};

				auto info = finalStagingAttachment.getDescriptorInfo(nullptr);
				descriptorSetUpdator.pushAttachment(info);
				descriptorSetUpdator.update();
			}
		}

		void createDrawCommands(){
			const ScopedCommand scopedCommand{batchDrawCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

			const auto renderPassInfo = batchDrawPass.getBeginInfo(batchFrameBuffer);

			vkCmdBeginRenderPass(scopedCommand, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			const auto pipelineContext = batchDrawPass.startCmdContext(scopedCommand);

			pipelineContext.data().bindDescriptorTo(scopedCommand);

			batchVertexData.bindTo(scopedCommand);

			vkCmdEndRenderPass(scopedCommand);
		}

		void createFlushCommands(){
			for(const auto& [i, commandBuffer] : swapChain.getCommandFlushes() | std::ranges::views::enumerate){
				ScopedCommand scopedCommand{commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				VkClearValue clearColor[1]{};
				auto renderPassInfo = flushPass.getBeginInfo(swapChain.getFrameBuffers()[i]);
				renderPassInfo.clearValueCount = 1;
				renderPassInfo.pClearValues = clearColor;

				vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, flushPass.at(0));

				vkCmdBindDescriptorSets(commandBuffer,
				                        VK_PIPELINE_BIND_POINT_GRAPHICS, flushPass.front().layout, 0,
				                        1, flushPass.front().descriptorSets[i].asData(),
				                        0, nullptr);

				commandBuffer.blitDraw();
				// vkCmdDraw(commandBuffer, 3, 1, 0, 0);

				vkCmdEndRenderPass(commandBuffer);

				batchFrameBuffer.atLocal(0).cmdClear(scopedCommand, {}, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
				//TODO self clear
				// postProcess.framebuffer.atLocal(0).cmdClear(scopedCommand, {}, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
				// postProcess.framebuffer.atLocal(1).cmdClear(scopedCommand, {}, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			}
		}

		void updateUniformBuffer(const UniformBlock& data) const{
			batchDrawPass.front().uniformBuffers[0].memory.loadData(data);
		}
	};
}

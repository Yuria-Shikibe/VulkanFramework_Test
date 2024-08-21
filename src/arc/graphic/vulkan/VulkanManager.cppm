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

		ColorAttachment blurResultAttachment{};
		ColorAttachment mergeResultAttachment{};
	    ColorAttachment nfaaResultAttachment{};

		std::atomic_bool descriptorSetUpdated{false};
		Fence batchFence{};
		CommandBuffer batchDrawCommand{};

		Graphic::PostProcessor processBlur{};
		Graphic::PostProcessor processBloom{};
		Graphic::PostProcessor nfaa{};

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


            processBlur.submitCommand({currentFrameData.renderFinishedSemaphore.get()});
            processBloom.submitCommand({processBlur.getToWait()});
            nfaa.submitCommand({processBloom.getToWait()});

            currentFrameData.inFlightFence.waitAndReset();
			const auto imageIndex = swapChain.acquireNextImage(currentFrameData.imageAvailableSemaphore);

			const std::array toWait{
					currentFrameData.imageAvailableSemaphore.get(), nfaa.getToWait()
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

            for(std::uint32_t i = 0; i < 2; ++i) {
                ColorAttachment baseColorAttachment{context.physicalDevice, context.device};
                baseColorAttachment.create(
                    swapChain.getTargetWindow()->getSize(), cmd,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                );

                batchFrameBuffer.pushCapturedAttachments(std::move(baseColorAttachment));
            }

			DepthStencilAttachment depthAttachment{context.physicalDevice, context.device};
			depthAttachment.create(
				swapChain.getTargetWindow()->getSize(), cmd,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			);

			batchFrameBuffer.pushCapturedAttachments(std::move(depthAttachment));

			blurResultAttachment = ColorAttachment{context.physicalDevice, context.device};
			blurResultAttachment.create(
					swapChain.size2D(), cmd,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
				);

			mergeResultAttachment = ColorAttachment{context.physicalDevice, context.device};
			mergeResultAttachment.create(
					swapChain.size2D(), cmd,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
				);

			nfaaResultAttachment = ColorAttachment{context.physicalDevice, context.device};
			nfaaResultAttachment.create(
					swapChain.size2D(), cmd,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
				);

			cmd = {};

            batchFrameBuffer.loadCapturedAttachments(0);
		    batchFrameBuffer.create();

		    bindSwapChainFrameBuffer();
        }

	    void bindSwapChainFrameBuffer() {
		    for(const auto& [i, group] : swapChain.getImageData() | std::ranges::views::enumerate){
		        group.framebuffer = FramebufferLocal{context.device, swapChain.size2D(), flushPass.renderPass};

		        group.framebuffer.loadExternalImageView({
                    {0, group.imageView.get()},
                    {1, nfaaResultAttachment.getView().get()}
                });
		        group.framebuffer.create();
		    }
		}

		void resize(SwapChain& swapChain){
            flushPass.resize(swapChain.size2D());
            batchDrawPass.resize(swapChain.size2D());

            auto cmd = obtainTransientCommand();
            blurResultAttachment.resize(swapChain.size2D(), cmd);
		    mergeResultAttachment.resize(swapChain.size2D(), cmd);
		    batchFrameBuffer.resize(swapChain.size2D(), cmd);
		    nfaaResultAttachment.resize(swapChain.size2D(), cmd);

            bindSwapChainFrameBuffer();

            cmd = {};

            processBloom.resize(swapChain.size2D());
		    processBlur.resize(swapChain.size2D());
		    nfaa.resize(swapChain.size2D());

		    updateInputDescriptorSet();

		    createDrawCommands();
		    createFlushCommands();
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
						.setShaderChain({&Assets::Shader::Vert::blitSingle, &Assets::Shader::Frag::blitSingle})
						.setStaticViewportAndScissor(pipeline.size().x, pipeline.size().y);

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			flushPass.resize(swapChain.size2D());
		}

		void setBatchGroup(){
			batchDrawPass.init(context);

			batchDrawPass.createRenderPass([](RenderPass& renderPass){
			    const auto ColorAttachmentDesc = VkAttachmentDescription{
                        .format = VK_FORMAT_R8G8B8A8_UNORM,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                    } | AttachmentDesc::Default |= AttachmentDesc::Stencil_DontCare;
				//Drawing attachment
				renderPass.pushAttachment(ColorAttachmentDesc);

			    //Light attachment
			    renderPass.pushAttachment(ColorAttachmentDesc);
			    //
			    // //data blemt
			    // renderPass.pushAttachment(ColorAttachmentDesc);

				renderPass.pushAttachment(VkAttachmentDescription{
						.format = VK_FORMAT_D24_UNORM_S8_UINT,
						.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
						.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
						.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
						.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
					} | AttachmentDesc::Default |= AttachmentDesc::Stencil_DontCare);

				renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
					subpass.addDependency(VkSubpassDependency{
							.dstSubpass = 0,
							.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
						} | Subpass::Dependency::External);

					subpass.addOutput(0);
					subpass.addOutput(1);
					subpass.addAttachment(
						RenderPass::AttachmentReference::Category::depthStencil, 2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
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
						.setDepthStencilState(Default::DefaultDepthStencilState)
						.setColorBlend(&Default::ColorBlending<std::array{
						    Blending::ScaledAlphaBlend,
						    Blending::ScaledAlphaBlend,
						    // Blending::AlphaBlend,
						}>)
						.setVertexInputInfo<VertBindInfo>()
						.setShaderChain({&Assets::Shader::Vert::batchShader, &Assets::Shader::Frag::batchShader})
						.setStaticScissors({{}, std::bit_cast<VkExtent2D>(pipeline.size())})
						.setStaticViewport(float(pipeline.size().x), float(pipeline.size().y));

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			batchDrawPass.resize(swapChain.size2D());
		}

		void createPostEffectPipeline(){
			processBlur = Assets::PostProcess::Factory::blurProcessorFactory.generate(swapChain.size2D(), [this]{
				Graphic::AttachmentPort port{};
				port.in.insert_or_assign(0u, batchFrameBuffer.atLocal(1).getView());
				port.out.insert_or_assign(3u, blurResultAttachment.getView());
				return port;
			});


			processBloom = Assets::PostProcess::Factory::mergeBloomFactory.generate(swapChain.size2D(), [this]{
				Graphic::AttachmentPort port{};
				port.in.insert_or_assign(0u, batchFrameBuffer.atLocal(1).getView());
				port.in.insert_or_assign(1u, blurResultAttachment.getView());
				port.out.insert_or_assign(2u, mergeResultAttachment.getView());

				return port;
			});

			nfaa = Assets::PostProcess::Factory::nfaaFactory.generate(swapChain.size2D(), [this]{
				Graphic::AttachmentPort port{};
				port.in.insert_or_assign(0u, nfaaResultAttachment.getView());

				return port;
			});

            nfaa.descriptorSetUpdator = [this](Graphic::PostProcessor &postProcessor) {
                auto &set = postProcessor.renderProcedure.front().descriptorSets;

                DescriptorSetUpdator updator{postProcessor.context->device, set.front()};

                auto info = Assets::Sampler::blitSampler.getDescriptorInfo_ShaderRead(
                    batchFrameBuffer.atLocal(0).getView(), VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
                updator.pushSampledImage(info);
                updator.update();
            };

		    nfaa.updateDescriptors();
		    nfaa.recordCommand();
        }

		void updateInputDescriptorSet(){
			for(std::size_t i = 0; i < swapChain.size(); ++i){
				DescriptorSetUpdator descriptorSetUpdator{context.device, flushPass.pipelinesLocal[0].descriptorSets[i]};

				auto info = nfaaResultAttachment.getDescriptorInfo(nullptr);
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

				vkCmdEndRenderPass(commandBuffer);

				batchFrameBuffer.atLocal(0).cmdClearColor(scopedCommand, {}, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
				batchFrameBuffer.atLocal(1).cmdClearColor(scopedCommand, {}, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
				batchFrameBuffer.atLocal(2).cmdClearDepthStencil(scopedCommand, {1.f}, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);


			}
		}

		void updateUniformBuffer(const UniformBlock& data) const{
			batchDrawPass.front().uniformBuffers[0].memory.loadData(data);
		}
	};
}

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
import Core.Vulkan.DescriptorBuffer;
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
import Core.Vulkan.RenderProcedure;
import Core.Vulkan.Attachment;

import Core.Vulkan.BatchData;
import Core.Vulkan.Util;

import Graphic.PostProcessor;

import Math;
import Core.File;

import Assets.Graphic;
import Assets.Graphic.PostProcess;

import ext.Event;

//TEMP
import Graphic.Renderer.World;

export namespace Core::Vulkan{
	struct ResizeEvent final : Geom::USize2, ext::EventType{

		[[nodiscard]] ResizeEvent(Geom::USize2 size2) : Geom::USize2(size2){}
	};

	class VulkanManager{
	public:
		[[nodiscard]] VulkanManager() = default;

		Graphic::RendererWorld* rendererWorld{};

		ext::EventManager eventManager{
			{ext::typeIndexOf<ResizeEvent>()}
		};

		std::function<std::pair<VkImageView, VkImageView>()> uiImageViewProv{};

		Context context{};
		CommandPool commandPool{};
		CommandPool transientCommandPool{};


		SwapChain swapChain{};

		RenderProcedure flushPass{};

		[[nodiscard]] TransientCommand obtainTransientCommand() const{
			return transientCommandPool.obtainTransient(context.device.getGraphicsQueue());
		}

		// FramebufferLocal batchFrameBuffer{};
	    // ImageView batchDepthImageView{};

		// ColorAttachment blurResultAttachment{};
		ColorAttachment gameMergeResultAttachment{};

		// Graphic::GraphicPostProcessor processBlur{};
		Graphic::GraphicPostProcessor processMerge{};
		Graphic::GraphicPostProcessor gameUIMerge{};
		Graphic::GraphicPostProcessor nfaaPostEffect{};

		DescriptorSetLayout cameraPropertyDescriptorLayout_Compute{};
		DescriptorBuffer cameraPropertyDescriptor{};
	    UniformBuffer cameraPropertiesUniformBuffer{};

		struct InFlightData{
			Fence inFlightFence{};
			Semaphore imageAvailableSemaphore{};
			Semaphore renderFinishedSemaphore{};
			Semaphore flushFinishedSemaphore{};
		};

		std::array<InFlightData, MAX_FRAMES_IN_FLIGHT> frameDataArr{};

		Graphic::BatchData batchVertexData{};

		template <std::regular_invocable<Geom::USize2> InitFunc>
		void registerResizeCallback(std::function<void(const ResizeEvent&)>&& callback, InitFunc initFunc) {
			initFunc(swapChain.size2D());
			eventManager.on<ResizeEvent>(std::move(callback));
		}

		void registerResizeCallback(std::function<void(const ResizeEvent&)>&& callback) {
			callback(ResizeEvent{swapChain.size2D()});
			eventManager.on<ResizeEvent>(std::move(callback));
		}

		void blitToScreen(){
			const auto currentFrame = swapChain.getCurrentInFlightImage();
			const auto& currentFrameData = frameDataArr[currentFrame];

			rendererWorld->post_gaussian();
            processMerge.submitCommand();

			//TODO wait ui draw semaphore
			nfaaPostEffect.submitCommand();
			gameUIMerge.submitCommand();

			currentFrameData.inFlightFence.waitAndReset();
			const auto imageIndex = swapChain.acquireNextImage(currentFrameData.imageAvailableSemaphore);

			const std::array toWait{currentFrameData.imageAvailableSemaphore.get()};


			const std::array cmds{swapChain.getCommandFlushes()[imageIndex].get()};

			context.commandSubmit_Graphics(
				VkSubmitInfo{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.waitSemaphoreCount = static_cast<std::uint32_t>(toWait.size()),
					.pWaitSemaphores = toWait.data(),
					.pWaitDstStageMask = Seq::StageFlagBits<
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT>,

					.commandBufferCount = cmds.size(),
					.pCommandBuffers = cmds.data(),

					.signalSemaphoreCount = 1,
					.pSignalSemaphores = currentFrameData.flushFinishedSemaphore.as_data(),
				}, currentFrameData.inFlightFence);

			VkPresentInfoKHR presentInfo{};

			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = currentFrameData.flushFinishedSemaphore.as_data();
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

			for (auto && commandBuffer : swapChain.getCommandFlushes()){
				commandBuffer = commandPool.obtain();
			}

		}

		void initPipeline(){
		    cameraPropertiesUniformBuffer = UniformBuffer{context.physicalDevice, context.device, sizeof(float)};

			cameraPropertyDescriptorLayout_Compute = DescriptorSetLayout{context.device, [](DescriptorSetLayout& layout){
				layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
			}};

			cameraPropertyDescriptor = DescriptorBuffer{context.physicalDevice, context.device,
				cameraPropertyDescriptorLayout_Compute,
				cameraPropertyDescriptorLayout_Compute.size()
			};

			cameraPropertyDescriptor.load([this](const DescriptorBuffer& buffer){
				buffer.loadUniform(0, cameraPropertiesUniformBuffer.getBufferAddress(), cameraPropertiesUniformBuffer.requestedSize());
			});

			setFlushGroup();
		    createFrameBuffers();
		    createPostEffectPipeline();

		    updateFlushInputDescriptorSet();

			bindSwapChainFrameBuffer();

			swapChain.recreateCallback = [this](SwapChain& swapChain){
				resize(swapChain);
			};


			createFrameObjects();
			createFlushCommands();

			std::cout.flush();
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

			gameMergeResultAttachment = ColorAttachment{context.physicalDevice, context.device};
			gameMergeResultAttachment.create(
					swapChain.size2D(), cmd,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
				);

			cmd = {};
        }

        void resize(SwapChain& swapChain){
			eventManager.fire(ResizeEvent{swapChain.size2D()});

            flushPass.resize(swapChain.size2D());

            auto cmd = obtainTransientCommand();
            gameMergeResultAttachment.resize(swapChain.size2D(), cmd);
            cmd = {};

			processMerge.resize(swapChain.size2D());
			nfaaPostEffect.resize(swapChain.size2D());
			gameUIMerge.resize(swapChain.size2D());

			updateFlushInputDescriptorSet();
			bindSwapChainFrameBuffer();

            createFlushCommands();
        }

        void bindSwapChainFrameBuffer() {
		    for(const auto& [i, group] : swapChain.getImageData() | std::ranges::views::enumerate){
		        group.framebuffer = FramebufferLocal{context.device, swapChain.size2D(), flushPass.renderPass};

		        group.framebuffer.loadExternalImageView({
                    {0, group.imageView.get()},
                    {1, gameUIMerge.framebuffer.atLocal(0).getView().get()}
                });
		        group.framebuffer.create();
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

			flushPass.pushAndInitPipeline([this](PipelineData& pipeline){
				pipeline.createDescriptorLayout([this](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
				});

				pipeline.createPipelineLayout();

				pipeline.createDescriptorSet(swapChain.size());

				pipeline.builder = [](PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.useDefaultFixedStages()
						.setShaderChain({&Assets::Shader::Vert::blitSingle, &Assets::Shader::Frag::blitSingle})
						.setStaticViewportAndScissor(pipeline.size.x, pipeline.size.y);

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			flushPass.resize(swapChain.size2D());
		}

		void createPostEffectPipeline(){

		    {
		        processMerge = Assets::PostProcess::Factory::mergeBloomFactory.generate(swapChain.size2D(), [this]{
                    Graphic::AttachmentPort port{};
                    port.in.insert_or_assign(0u, rendererWorld->baseColor.getView());
                    port.in.insert_or_assign(1u, rendererWorld->lightColor.getView());
                    port.in.insert_or_assign(2u, rendererWorld->getGaussianResult().getView());
                    // port.in.insert_or_assign(3u, batchFrameBuffer.atLocal(2).getView());

                    port.out.insert_or_assign(4u, gameMergeResultAttachment.getView());

                    return port;
                });

			    processMerge.descriptorSetUpdator = [this](Graphic::GraphicPostProcessor& postProcessor){
			        {
			            auto& set = postProcessor.renderProcedure.atLocal(0).descriptorSets;

			            DescriptorSetUpdator updator{postProcessor.context->device, set.front()};

                        const auto& ubo = postProcessor.renderProcedure.atLocal(0).uniformBuffers[0];

			            auto* data = static_cast<Assets::PostProcess::UniformBlock_kernalSSAO *>(ubo.memory.map());
			            new (data) Assets::PostProcess::UniformBlock_kernalSSAO;

			            const auto scale = ~postProcessor.size().as<float>();

			            for (std::size_t count{}; const auto & param : Assets::PostProcess::UniformBlock_kernalSSAO::SSAO_Params) {
                            for(std::size_t i = 0; i < param.count; ++i) {
                                data->kernal[count].off.setPolar((Math::DEG_FULL / static_cast<float>(param.count)) * static_cast<float>(i), param.distance) *= scale;
                                data->kernal[count].weight = param.weight;
                                count++;
                            }
			            }

			            ubo.memory.unmap();

			            auto uniformInfo1 = ubo.getDescriptorInfo();
			            auto uniformInfo2 = cameraPropertiesUniformBuffer.getDescriptorInfo();
			            auto depthInfo = VkDescriptorImageInfo{
			                .sampler = Assets::Sampler::blitSampler,
                            .imageView = rendererWorld->depthAttachmentView,
                            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                        };

			            updator.push(uniformInfo1);
                        updator.pushSampledImage(depthInfo);
                        updator.push(uniformInfo2);
                        updator.update();
			        }

			        {
			            auto& set = postProcessor.renderProcedure.atLocal(1).descriptorSets;

			            DescriptorSetUpdator updator{postProcessor.context->device, set.front()};

			            std::array layouts{
			                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        };

			            std::array<VkDescriptorImageInfo, std::ssize(layouts)> infos{};

			            for(const auto& [index, layout] : layouts | std::views::enumerate) {
			                infos[index] = postProcessor.framebuffer.getInputInfo(index, layout);
			            }

			            for (auto && [index, inputAttachmentImageInfo] : infos | std::views::enumerate) {
			                updator.pushAttachment(inputAttachmentImageInfo);
			            }

			            updator.update();
			        }

			    };

			    processMerge.commandRecorder = [this](Graphic::GraphicPostProcessor& postProcessor){
			        ScopedCommand scopedCommand{postProcessor.commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

			        rendererWorld->depthStencilAttachment.getImage().transitionImageLayout(
                    scopedCommand, {
                        .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        .srcAccessMask = 0,
                        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                    }, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

			        const auto info = postProcessor.renderProcedure.getBeginInfo(postProcessor.framebuffer);
			        vkCmdBeginRenderPass(scopedCommand, &info, VK_SUBPASS_CONTENTS_INLINE);
                    auto cmdContext = postProcessor.renderProcedure.startCmdContext(scopedCommand);

                    cmdContext.data().bindDescriptorTo(scopedCommand);
			        scopedCommand->blitDraw();

			        cmdContext.next();

			        cmdContext.data().bindDescriptorTo(scopedCommand);
			        scopedCommand->blitDraw();

			        vkCmdEndRenderPass(scopedCommand);

                    processMerge.framebuffer.atLocal(0).getImage().transitionImageLayout(
                        scopedCommand, {
                            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            .srcAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                        }, VK_IMAGE_ASPECT_COLOR_BIT);

			    };

			    processMerge.updateDescriptors();
			    processMerge.recordCommand();
		    }

			{
				nfaaPostEffect = Assets::PostProcess::Factory::nfaaFactory.generate(swapChain.size2D(), {});

            	nfaaPostEffect.descriptorSetUpdator = [this](Graphic::GraphicPostProcessor &postProcessor) {
            		auto &set = postProcessor.renderProcedure.front().descriptorSets;

            		DescriptorSetUpdator updator{postProcessor.context->device, set.front()};

            		auto info = Assets::Sampler::blitSampler.getDescriptorInfo_ShaderRead(
						gameMergeResultAttachment.getView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            		updator.pushSampledImage(info);
            		updator.update();
            	};

            	nfaaPostEffect.updateDescriptors();
            	nfaaPostEffect.recordCommand();
			}


			{
            	gameUIMerge = Assets::PostProcess::Factory::game_uiMerge.generate(swapChain.size2D(), [this]{
					auto [ui1, ui2] = uiImageViewProv();
					Graphic::AttachmentPort port{};
					port.in.insert_or_assign(0u, nfaaPostEffect.framebuffer.atLocal(0).getView());
					port.in.insert_or_assign(1u, ui1);
					port.in.insert_or_assign(2u, ui2);
					// port.in.insert_or_assign(3u, batchFrameBuffer.atLocal(2).getView());

					return port;
				});
			}
        }

		void updateFlushInputDescriptorSet(){
			for(std::size_t i = 0; i < swapChain.size(); ++i){
				DescriptorSetUpdator descriptorSetUpdator{context.device, flushPass.pipelinesLocal[0].descriptorSets[i]};

				auto info = gameUIMerge.framebuffer.atLocal(0).getDescriptorInfo(nullptr);
				descriptorSetUpdator.pushAttachment(info);
				descriptorSetUpdator.update();
			}
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
				                        1, flushPass.front().descriptorSets[i].as_data(),
				                        0, nullptr);

				commandBuffer.blitDraw();

				vkCmdEndRenderPass(commandBuffer);

				rendererWorld->baseColor.cmdClearColor(scopedCommand, {}, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
				rendererWorld->lightColor.cmdClearColor(scopedCommand, {}, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
				rendererWorld->depthStencilAttachment.cmdClearDepthStencil(scopedCommand, {1.f}, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
			}
		}

		void updateCameraScale(const float scale) const{
		    cameraPropertiesUniformBuffer.memory.loadData(scale);
		}
	};
}

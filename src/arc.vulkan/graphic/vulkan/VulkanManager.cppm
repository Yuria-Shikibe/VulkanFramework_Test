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
import Core.Vulkan.RenderProcedure;
import Core.Vulkan.Attachment;
import Core.Vulkan.BatchData;

import Graphic.PostProcessor;

import Math;
import Core.File;

import Assets.Graphic;
import Assets.Graphic.PostProcess;

import ext.Event;

export namespace Core::Vulkan{
	struct ResizeEvent final : Geom::USize2, ext::EventType{

		[[nodiscard]] ResizeEvent(Geom::USize2 size2) : Geom::USize2(size2){}
	};

	class VulkanManager{
	public:
		[[nodiscard]] VulkanManager() = default;

		ext::EventManager eventManager{
			{ext::typeIndexOf<ResizeEvent>()}
		};

		std::function<std::pair<VkImageView, VkImageView>()> uiImageViewProv{};

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
	    ImageView batchDepthImageView{};

		ColorAttachment blurResultAttachment{};
		ColorAttachment gameMergeResultAttachment{};
		CommandBuffer batchDrawCommand{};

		Graphic::GraphicPostProcessor processBlur{};
		Graphic::GraphicPostProcessor processMerge{};
		Graphic::GraphicPostProcessor gameUIMerge{};
		Graphic::GraphicPostProcessor nfaaPostEffect{};

	    UniformBuffer cameraScaleUniformBuffer{};

		struct InFlightData{
			Fence inFlightFence{};
			Semaphore imageAvailableSemaphore{};
			Semaphore renderFinishedSemaphore{};
			Semaphore flushFinishedSemaphore{};
		};

		std::array<InFlightData, MAX_FRAMES_IN_FLIGHT> frameDataArr{};

		BatchData batchVertexData{};

		template <std::regular_invocable<Geom::USize2> InitFunc>
		void registerResizeCallback(std::function<void(const ResizeEvent&)>&& callback, InitFunc initFunc) {
			initFunc(swapChain.size2D());
			eventManager.on<ResizeEvent>(std::move(callback));
		}

		void registerResizeCallback(std::function<void(const ResizeEvent&)>&& callback) {
			callback(ResizeEvent{swapChain.size2D()});
			eventManager.on<ResizeEvent>(std::move(callback));
		}


		void drawFrame(const Fence& batchFence) const{
			context.commandSubmit_Graphics(batchDrawCommand, batchFence.get());
		}

		void blitToScreen(){
			const auto currentFrame = swapChain.getCurrentInFlightImage();
			const auto& currentFrameData = frameDataArr[currentFrame];

            processBlur.submitCommand({});
            processMerge.submitCommand({});

			//TODO wait ui draw semaphore
			nfaaPostEffect.submitCommand({});
			gameUIMerge.submitCommand({});

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

			batchDrawCommand = commandPool.obtain();
			for (auto && commandBuffer : swapChain.getCommandFlushes()){
				commandBuffer = commandPool.obtain();
			}

		}

		void initPipeline(){
		    cameraScaleUniformBuffer = UniformBuffer{context.physicalDevice, context.device, sizeof(float)};

			setFlushGroup();
			setBatchGroup();
		    createFrameBuffers();
		    createDepthView();
		    createPostEffectPipeline();

		    updateFlushInputDescriptorSet();

			bindSwapChainFrameBuffer();

			swapChain.recreateCallback = [this](SwapChain& swapChain){
				resize(swapChain);
			};


			createFrameObjects();
			createFlushCommands();

			updateBatchDescriptorSet({}, nullptr);
			createBatchDrawCommands();

			std::cout.flush();
		}

		void updateBatchDescriptorSet(std::span<const VkImageView> imageViews, const Fence* batchFence){
			DescriptorSetUpdator updator{context.device, batchDrawPass.front().descriptorSets.front()};

			auto bufferInfo = batchDrawPass.front().uniformBuffers.front().getDescriptorInfo();
		    auto imageInfo =
                Util::getDescriptorInfoRange_ShaderRead(Assets::Sampler::textureNearestSampler, imageViews);

			updator.push(bufferInfo);
			if(!imageInfo.empty())updator.push(imageInfo);

		    if(batchFence)batchFence->waitAndReset();

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
			batchFrameBuffer = FramebufferLocal{context.device, swapChain.size2D(), batchDrawPass.renderPass};

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

			gameMergeResultAttachment = ColorAttachment{context.physicalDevice, context.device};
			gameMergeResultAttachment.create(
					swapChain.size2D(), cmd,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
				);

			cmd = {};

            batchFrameBuffer.loadCapturedAttachments(0);
		    batchFrameBuffer.create();
        }

        void resize(SwapChain& swapChain){
			eventManager.fire(ResizeEvent{swapChain.size2D()});

            flushPass.resize(swapChain.size2D());
            batchDrawPass.resize(swapChain.size2D());

            auto cmd = obtainTransientCommand();
            blurResultAttachment.resize(swapChain.size2D(), cmd);
            gameMergeResultAttachment.resize(swapChain.size2D(), cmd);
            batchFrameBuffer.resize(swapChain.size2D(), cmd);
            cmd = {};

            createDepthView();

            processMerge.resize(swapChain.size2D());
            processBlur.resize(swapChain.size2D());
			nfaaPostEffect.resize(swapChain.size2D());
			gameUIMerge.resize(swapChain.size2D());

			updateFlushInputDescriptorSet();
			bindSwapChainFrameBuffer();

            createBatchDrawCommands();
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

			batchDrawPass.pushAndInitPipeline([](PipelineData& pipeline){
				pipeline.createDescriptorLayout([](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (8));

					// layout.bindingFlags = {
					// 	VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
					// 	VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT};
					// layout.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
				});

				pipeline.createPipelineLayout();

				pipeline.createDescriptorSet(1);
				pipeline.addUniformBuffer(sizeof(UniformBlock));

				pipeline.builder = [](PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.useDefaultFixedStages()
						.setDepthStencilState(Default::DefaultDepthStencilState)
						.setColorBlend(&Default::ColorBlending<std::array{
						    Blending::AlphaBlend,
						    Blending::ScaledAlphaBlend,
						    // Blending::AlphaBlend,
						}>)
						.setVertexInputInfo<WorldVertBindInfo>()
						.setShaderChain({&Assets::Shader::Vert::batchShader, &Assets::Shader::Frag::batchShader})
						.setStaticScissors({{}, std::bit_cast<VkExtent2D>(pipeline.size)})
						.setStaticViewport(float(pipeline.size.x), float(pipeline.size.y));

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			batchDrawPass.resize(swapChain.size2D());
		}

		void createPostEffectPipeline(){
            {
                processBlur = Assets::PostProcess::Factory::blurProcessorFactory.generate(swapChain.size2D(), [this] {
                    Graphic::AttachmentPort port{};
                    port.in.insert_or_assign(0u, batchFrameBuffer.atLocal(1).getView());
                    port.out.insert_or_assign(3u, blurResultAttachment.getView());
                    return port;
                });

                processBlur.descriptorSetUpdator = [this](Graphic::GraphicPostProcessor &postProcessor) {
                    auto &set = postProcessor.renderProcedure.front().descriptorSets;

                    constexpr std::array layouts{
                            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                        };

                    for(std::size_t i = 0; i < layouts.size(); ++i) {
                        DescriptorSetUpdator updator{postProcessor.context->device, set[i]};
                        auto imageInfo = Assets::Sampler::blitSampler.getDescriptorInfo_ShaderRead(
                            postProcessor.framebuffer.at(i),
                            layouts[i]
                            );
                        auto uniformInfo = cameraScaleUniformBuffer.getDescriptorInfo();

                        updator.pushSampledImage(imageInfo);
                        updator.push(uniformInfo);
                        updator.update();
                    }
                };

                processBlur.updateDescriptors();
                processBlur.recordCommand();
            }

		    {
		        processMerge = Assets::PostProcess::Factory::mergeBloomFactory.generate(swapChain.size2D(), [this]{
                    Graphic::AttachmentPort port{};
                    port.in.insert_or_assign(0u, batchFrameBuffer.atLocal(0).getView());
                    port.in.insert_or_assign(1u, batchFrameBuffer.atLocal(1).getView());
                    port.in.insert_or_assign(2u, blurResultAttachment.getView());
                    // port.in.insert_or_assign(3u, batchFrameBuffer.atLocal(2).getView());

                    port.out.insert_or_assign(4u, gameMergeResultAttachment.getView());

                    return port;
                });

			    processMerge.descriptorSetUpdator = [this](Graphic::GraphicPostProcessor& postProcessor){
			        {
			            auto& set = postProcessor.renderProcedure.atLocal(0).descriptorSets;

			            DescriptorSetUpdator updator{postProcessor.context->device, set.front()};

                        const auto& ubo = postProcessor.renderProcedure.atLocal(0).uniformBuffers[0];

			            auto* data = static_cast<Assets::PostProcess::UniformBlock_SSAO *>(ubo.memory.map());
			            new (data) Assets::PostProcess::UniformBlock_SSAO;

			            const auto scale = ~postProcessor.size().as<float>();

			            for (std::size_t count{}; const auto & param : Assets::PostProcess::UniformBlock_SSAO::SSAO_Params) {
                            for(std::size_t i = 0; i < param.count; ++i) {
                                data->kernal[count].first.setPolar((Math::DEG_FULL / static_cast<float>(param.count)) * static_cast<float>(i), param.distance) *= scale;
                                data->kernal[count].second.x = param.weight;
                                count++;
                            }
			            }

			            ubo.memory.unmap();

			            auto uniformInfo1 = ubo.getDescriptorInfo();
			            auto uniformInfo2 = cameraScaleUniformBuffer.getDescriptorInfo();
			            auto depthInfo = VkDescriptorImageInfo{
			                .sampler = Assets::Sampler::blitSampler,
                            .imageView = batchDepthImageView,
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

			        batchFrameBuffer.atLocal(2).getImage().transitionImageLayout(
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

	    void createDepthView() {
		    batchDepthImageView = ImageView{context.device, {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = batchFrameBuffer.atLocal(2).getImage(),
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = VK_FORMAT_D24_UNORM_S8_UINT,
                .components = {},
                .subresourceRange = VkImageSubresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
		    }};
		}

		void createBatchDrawCommands(){
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

		void updateBatchUniformBuffer(const UniformBlock& data) const{
			batchDrawPass.front().uniformBuffers[0].memory.loadData(data);
		}

		void updateCameraScale(const float scale) const{
		    cameraScaleUniformBuffer.memory.loadData(scale);
		}
	};
}

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
import Core.Vulkan.RenderPass;

import Core.Vulkan.CommandPool;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Buffer.IndexBuffer;
import Core.Vulkan.Buffer.VertexBuffer;
import Core.Vulkan.Buffer.FrameBuffer;
import Core.Vulkan.Sampler;
import Core.Vulkan.Comp;
import Core.Vulkan.RenderPassGroup;
import Core.Vulkan.Attachment;

import OS.File;


import Assets.Graphic;

export namespace Core::Vulkan{
	class VulkanManager{
	public:
		[[nodiscard]] VulkanManager() = default;

		Context context{};

		CommandPool commandPool{};
		CommandPool transientCommandPool{};


		SwapChain swapChain{};

		RenderPassGroup batchDrawPass{};
		RenderPassGroup flushPass{};

		DescriptorSetLayout descriptorSetLayout{};
		DescriptorSetPool descriptorPool{};
		UniformBuffer uniformBuffer{}; //TODO
		DescriptorSet descriptorSet{}; //TODO

		TransientCommand obtainTransientCommand() const{
			return transientCommandPool.obtainTransient(context.device.getGraphicsQueue());
		}

		FramebufferLocal batchLocal{};

		std::atomic_bool reseted{false};
		Fence batchFence{};

		struct InFlightData{

			Fence inFlightFence{};
			Semaphore imageAvailableSemaphore{};
			Semaphore renderFinishedSemaphore{};
			Semaphore flushFinishedSemaphore{};


			CommandBuffer vertexFlushCommandBuffer{};
		};

		std::array<InFlightData, MAX_FRAMES_IN_FLIGHT> frameDataArr{};

		struct BatchVertexData{
			VkBuffer vertexBuffer{};
			VkBuffer indexBuffer{};
			VkIndexType indexType{};
			VkBuffer indirectBuffer{};
		} batchVertexData{};

		void drawFrame(VkSemaphore toWait, VkSemaphore toSignal, bool isLast){
			auto currentFrame = swapChain.getCurrentInFlightImage();
			auto& currentFrameData = frameDataArr[currentFrame];
			auto imageIndex = swapChain.getCurrentImageIndex();

			const std::array semaphoresToWait{toWait};

			VkSubmitInfo submitInfo{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.waitSemaphoreCount = static_cast<std::uint32_t>(semaphoresToWait.size()),
					.pWaitSemaphores = semaphoresToWait.data(),
					.pWaitDstStageMask = Seq::StageFlagBits<VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT>,
					.commandBufferCount = 1u,
					.pCommandBuffers = swapChain.getCommandDraw()[imageIndex].asData(),
				};

			if(reseted){
				reseted = false;
			}else{
				batchFence.waitAndReset();
			}

			if(isLast){
				const std::array semaphoresToSignal{toSignal, currentFrameData.renderFinishedSemaphore.get()};

				// currentFrameData.inFlightFence.reset();
				submitInfo.signalSemaphoreCount = semaphoresToSignal.size();
				submitInfo.pSignalSemaphores = semaphoresToSignal.data();

				context.commandSubmit_Graphics(submitInfo, batchFence);
			} else{
				const std::array semaphoresToSignal{toSignal};

				submitInfo.signalSemaphoreCount = semaphoresToSignal.size();
				submitInfo.pSignalSemaphores = semaphoresToSignal.data();

				context.commandSubmit_Graphics(submitInfo, batchFence);
			}
		}

		void drawEnd(){
			auto currentFrame = swapChain.getCurrentInFlightImage();
			auto& currentFrameData = frameDataArr[currentFrame];

			currentFrameData.inFlightFence.waitAndReset();
			const auto imageIndex = swapChain.acquireNextImage(currentFrameData.imageAvailableSemaphore);

			const std::array toWait{
					currentFrameData.imageAvailableSemaphore.get(), currentFrameData.renderFinishedSemaphore.get()
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

			context.selectPhysicalDevice(swapChain.getSurface());
			context.device = LogicalDevice{context.physicalDevice, context.physicalDevice.queues};

			swapChain.createSwapChain(context.physicalDevice, context.device);
			swapChain.presentQueue = context.device.getPresentQueue();

			commandPool = CommandPool{
					context.device, context.physicalDevice.queues.graphicsFamily,
					VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
				};
			transientCommandPool = CommandPool{
					context.device, context.physicalDevice.queues.graphicsFamily, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
				};

			batchFence = Fence{context.device, Fence::CreateFlags::signal};
		}

		void initVulkan(){
			setFlushGroup();
			setBatchGroup();

			createFrameBuffers();
			updateInputDescriptorSet();

			swapChain.recreateCallback = [this](SwapChain& swapChain){
				//TODO createGraphicsPipeline();
				createFrameBuffers();

				flushPass.resize(swapChain.getTargetWindow()->getSize());
				batchDrawPass.resize(swapChain.getTargetWindow()->getSize());

				updateInputDescriptorSet();

				createDrawCommands();
				createFlushCommands();
			};

			descriptorSetLayout = DescriptorSetLayout{
					context.device, [](DescriptorSetLayout& layout){
						layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
						layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (8));
					}
				};
			descriptorPool = DescriptorSetPool{context.device, descriptorSetLayout, 1};
			descriptorSet = descriptorPool.obtain();
			uniformBuffer = UniformBuffer{context.physicalDevice, context.device, sizeof(UniformBlock)};

			createFrameObjects();

			// createClearCommands();
			// createClearCommands();
			createFlushCommands();

			std::cout.flush();
		}

		void updateDescriptorSet(std::span<VkDescriptorImageInfo> imageInfo){

			DescriptorSetUpdator updator{context.device, descriptorSet};

			auto bufferInfo = uniformBuffer.getDescriptorInfo();

			updator.push(bufferInfo);
			if(!imageInfo.empty())updator.push(imageInfo);

			if(!imageInfo.empty()){
				batchFence.waitAndReset();
				reseted = true;
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
				// frameData.clearSemaphore = Semaphore{context.device};
				// frameData.vertexFlushSemaphore = Semaphore{context.device};
				frameData.vertexFlushCommandBuffer = commandPool.obtain();
			}
		}

		void createFrameBuffers(){

			batchLocal.setDevice(context.device);
			batchLocal.setSize(swapChain.getTargetWindow()->getSize());
			batchLocal.localAttachments.clear();

			Attachment colorAttachment{context.physicalDevice, context.device};
			colorAttachment.create(
				swapChain.getTargetWindow()->getSize(), obtainTransientCommand(),
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
			);
			batchLocal.pushCapturedAttachments(std::move(colorAttachment));

			batchLocal.loadCapturedAttachments(1);
			batchLocal.createFrameBuffer(batchDrawPass.renderPass);

			for(const auto& [i, group] : swapChain.getImageData() | std::views::enumerate){
				group.framebuffer.setDevice(context.device);
				group.framebuffer.setSize(swapChain.getTargetWindow()->getSize());

				group.framebuffer.loadExternalImageView({{0, group.imageView.get()}, {1, batchLocal.localAttachments[0].getView().get()}});
				group.framebuffer.createFrameBuffer(flushPass.renderPass);
			}
		}

	public:
		void recordCommands(){
			createDrawCommands();
			// createClearCommands();
		}

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
						.format = swapChain.getFormat(),
						.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
						.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					} | AttachmentDesc::Default |= AttachmentDesc::Stencil_DontCare);

				renderPass.pushSubpass([](RenderPass::SubpassData& subpassData){
					subpassData.setProperties(VK_PIPELINE_BIND_POINT_GRAPHICS);

					subpassData.addDependency(VkSubpassDependency{
							.srcSubpass = subpassData.index,
							.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
							.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
							.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT | VK_DEPENDENCY_VIEW_LOCAL_BIT
						});

					subpassData.addAttachment(
						RenderPass::AttachmentReference::Category::color, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
					subpassData.addAttachment(
						RenderPass::AttachmentReference::Category::input, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
				});
			});

			flushPass.rebuildPipelineFunc = [this](RenderPassGroup& flushPass){
				flushPass.pushAndInitPipeline([this](RenderPassGroup::PipelineData& pipeline){

					pipeline.setDescriptorLayout([this](DescriptorSetLayout& layout){
						// layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
						layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
						//
						// layout.pushBindingFlag(VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT);
						// layout.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
					});

					pipeline.setPipelineLayout();

					pipeline.createDescriptorSet(swapChain.size());

					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.useDefaultFixedStages()
						.setVertexInputInfo<Util::EmptyVertexBind>()
						.setShaderChain({&Assets::Shader::blitSingleVert, &Assets::Shader::blitSingleFrag})
						.setStaticScissors({{}, std::bit_cast<VkExtent2D>(pipeline.group->size)})
						.setStaticViewport(float(pipeline.group->size.x), float(pipeline.group->size.y));

					pipeline.createPipeline(pipelineTemplate);
				});
			};

			flushPass.resize(swapChain.getTargetWindow()->getSize());
		}

		void setBatchGroup(){
			batchDrawPass.init(context);

			batchDrawPass.createRenderPass([this](RenderPass& renderPass){
				//Drawing attachment
				renderPass.pushAttachment(VkAttachmentDescription{
						.format = VK_FORMAT_R8G8B8A8_UNORM,
						.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
						.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
						.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
					} | AttachmentDesc::Default |= AttachmentDesc::Stencil_DontCare);


				renderPass.pushSubpass([](RenderPass::SubpassData& subpassData){
					subpassData.setProperties(VK_PIPELINE_BIND_POINT_GRAPHICS);

					subpassData.addDependency(VkSubpassDependency{
							.dstSubpass = 0,
							.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
						} | Subpass::Dependency::External);

					subpassData.addAttachment(
						RenderPass::AttachmentReference::Category::color, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				});
			});

			batchDrawPass.rebuildPipelineFunc = [](RenderPassGroup& batchDrawPass){
				batchDrawPass.pushAndInitPipeline([](RenderPassGroup::PipelineData& pipeline){
					pipeline.setDescriptorLayout([](DescriptorSetLayout& layout){
						layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
						layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (8));

						//
						// layout.pushBindingFlag(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
						// layout.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
					});

					pipeline.setPipelineLayout();

					pipeline.createDescriptorSet(1);

					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.useDefaultFixedStages()
						.setVertexInputInfo<VertBindInfo>()
						.setShaderChain({&Assets::Shader::batchVertShader, &Assets::Shader::batchFragShader})
						.setStaticScissors({{}, std::bit_cast<VkExtent2D>(pipeline.group->size)})
						.setStaticViewport(float(pipeline.group->size.x), float(pipeline.group->size.y));

					pipeline.createPipeline(pipelineTemplate);
				});
			};
			batchDrawPass.resize(swapChain.getTargetWindow()->getSize());
		}

		void updateInputDescriptorSet(){
			for(std::size_t i = 0; i < swapChain.size(); ++i){
				DescriptorSetUpdator descriptorSetUpdator{context.device, flushPass.pipelines[0].descriptorSets[i]};

				// auto info = batchLocal.localAttachments[0].getDescriptorInfo(Assets::Sampler::textureDefaultSampler);
				auto info = batchLocal.localAttachments[0].getDescriptorInfo(nullptr);
				descriptorSetUpdator.pushAttachment(info);
				descriptorSetUpdator.update();
			}
		}

		void createDrawCommands(){
			for(const auto& [i, commandBuffer] : swapChain.getCommandDraw() | std::views::enumerate){
				commandBuffer = commandPool.obtain();

				ScopedCommand scopedCommand{commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				VkRenderPassBeginInfo renderPassInfo{
						.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
						.pNext = nullptr,
						.renderPass = batchDrawPass.renderPass,
						.framebuffer = batchLocal,
						.renderArea = {{}, swapChain.getExtent()},
					};

				vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, batchDrawPass.pipelines[0].pipeline);

				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &batchVertexData.vertexBuffer, Seq::NoOffset);
				vkCmdBindIndexBuffer(commandBuffer, batchVertexData.indexBuffer, 0, batchVertexData.indexType);
				vkCmdBindDescriptorSets(commandBuffer,
				                        VK_PIPELINE_BIND_POINT_GRAPHICS, batchDrawPass.pipelines[0].layout, 0,
				                        1, descriptorSet.asData(),
				                        0, nullptr);
				vkCmdDrawIndexedIndirect(commandBuffer, batchVertexData.indirectBuffer, 0, 1,
				                         sizeof(VkDrawIndexedIndirectCommand));

				vkCmdEndRenderPass(commandBuffer);
			}
		}

		void createFlushCommands(){
			for(const auto& [i, commandBuffer] : swapChain.getCommandFlushes() | std::views::enumerate){
				commandBuffer = commandPool.obtain();

				ScopedCommand scopedCommand{commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				VkClearValue clearColor[1]{};
				VkRenderPassBeginInfo renderPassInfo{
						.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
						.pNext = nullptr,
						.renderPass = flushPass.renderPass,
						.framebuffer = swapChain.getFrameBuffers()[i],
						.renderArea = {{}, swapChain.getExtent()},
						.clearValueCount = 1,
						.pClearValues = clearColor
					};

				vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, flushPass.pipelines[0].pipeline);

				vkCmdBindDescriptorSets(commandBuffer,
				                        VK_PIPELINE_BIND_POINT_GRAPHICS, flushPass.pipelines[0].layout, 0,
				                        1, flushPass.pipelines[0].descriptorSets[i].asData(),
				                        0, nullptr);

				vkCmdDraw(commandBuffer, 3, 1, 0, 0);

				VkClearAttachment clearAttachment = {};
				clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				clearAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

				VkClearRect clearRect = {};
				clearRect.rect.offset = {0, 0};
				clearRect.rect.extent = swapChain.getExtent();
				clearRect.baseArrayLayer = 0;
				clearRect.layerCount = 1;

				vkCmdEndRenderPass(commandBuffer);

				batchLocal.localAttachments[0].cmdClear(commandBuffer, {},
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
			}
		}

		void updateUniformBuffer(const UniformBlock& data) const{
			uniformBuffer.memory.loadData(data);
		}
	};
}

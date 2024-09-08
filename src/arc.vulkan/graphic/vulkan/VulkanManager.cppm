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

		[[nodiscard]] explicit ResizeEvent(const Geom::USize2 size2) : Geom::USize2(size2){}
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

		Graphic::ComputePostProcessor presentMerge{};

		SwapChain swapChain{};
		RenderProcedure flushPass{};

		[[nodiscard]] TransientCommand obtainTransientCommand() const{
			return transientCommandPool.obtainTransient(context.device.getGraphicsQueue());
		}

		struct InFlightData{
			Fence inFlightFence{};

			Semaphore imageAvailableSemaphore{};
			Semaphore renderFinishedSemaphore{};
			Semaphore flushFinishedSemaphore{};
		};

		std::array<InFlightData, MAX_FRAMES_IN_FLIGHT> frameDataArr{};

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

			rendererWorld->doPostProcess();
			presentMerge.submitCommand();

			currentFrameData.inFlightFence.waitAndReset();
			const auto imageIndex = swapChain.acquireNextImage(currentFrameData.imageAvailableSemaphore);

			Util::submitCommand(
				context.device.getGraphicsQueue(),
				swapChain.getCommandFlushes()[imageIndex],
				currentFrameData.inFlightFence,
				currentFrameData.imageAvailableSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				currentFrameData.flushFinishedSemaphore, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT
			);

			swapChain.postImage(imageIndex, currentFrameData.flushFinishedSemaphore);
		}

		void initContext(Window* window){
			context.init();

			swapChain.attachWindow(window);
			swapChain.createSurface(context.instance);

			context.createDevice(swapChain.getSurface());

			swapChain.createSwapChain(context.physicalDevice, context.device);
			swapChain.presentQueue = context.device.getPresentQueue();

			commandPool = CommandPool{
					context.device, context.graphicFamily(),
					VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
				};

			transientCommandPool = CommandPool{
					context.device, context.graphicFamily(),
				VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
				};

			for (auto && commandBuffer : swapChain.getCommandFlushes()){
				commandBuffer = commandPool.obtain();
			}

			createFrameObjects();
		}

		void initPipeline(){
			setFlushGroup();

			{
				presentMerge = Assets::PostProcess::Factory::presentMerge.generate(swapChain.size2D(), [this]{
					Graphic::AttachmentPort port{};

					auto [ui1, ui2] = uiImageViewProv();

					port.in.insert_or_assign(0, rendererWorld->getResult_NFAA().getView());
					port.in.insert_or_assign(1, ui1);
					port.in.insert_or_assign(2, ui2);

					return port;
				});
			}


		    updateFlushInputDescriptorSet();
			bindSwapChainFrameBuffer();

			swapChain.recreateCallback = [this](const SwapChain& swapChain){
				resize(swapChain);
			};

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

        void resize(const SwapChain& swapChain){
			eventManager.fire(ResizeEvent{swapChain.size2D()});

			presentMerge.resize(swapChain.size2D());

            flushPass.resize(swapChain.size2D());

			bindSwapChainFrameBuffer();
			updateFlushInputDescriptorSet();

            createFlushCommands();
        }

        void bindSwapChainFrameBuffer() {
		    for(const auto& [i, group] : swapChain.getImageData() | std::ranges::views::enumerate){
		        group.framebuffer = FramebufferLocal{context.device, swapChain.size2D(), flushPass.renderPass};

		        group.framebuffer.loadExternalImageView({
                    {0, group.imageView.get()},
                    {1, presentMerge.images.back().getView().get()}
                });
		        group.framebuffer.create();
		    }
		}

    public:
		auto& getFinalAttachment(){
			return presentMerge.images.back();
			// return rendererWorld->getResult_NFAA();
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
						.format = VK_FORMAT_R8G8B8A8_UNORM,
						.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
						.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					} | AttachmentDesc::Default |= AttachmentDesc::Stencil_DontCare);

				renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
					subpass.addOutput(0);
					subpass.addInput(1);
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
						.setColorBlend(&Default::ColorBlending<std::array{Blending::Disable}>)
						.setShaderChain({&Assets::Shader::Vert::blitSingle, &Assets::Shader::Frag::blitSingle})
						.setStaticViewportAndScissor(pipeline.size.x, pipeline.size.y);

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			flushPass.resize(swapChain.size2D());
		}

		void updateFlushInputDescriptorSet(){
			for(std::size_t i = 0; i < swapChain.size(); ++i){
				DescriptorSetUpdator descriptorSetUpdator{context.device, flushPass.pipelinesLocal.front().descriptorSets[i]};

				auto info = getFinalAttachment().getDescriptorInfo(nullptr);
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
			}
		}

		// void createPresentMergeData(){
		// 	presentPipelineData = SinglePipelineData{&context};
		// 	presentPipelineData.createDescriptorLayout([](DescriptorSetLayout& layout){
		// 		layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
		// 		layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
		// 		// layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
		// 		// layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
		// 		layout.builder.push_seq(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
		// 	});
		//
		// 	presentPipelineData.createPipelineLayout();
		// 	presentPipelineData.createComputePipeline(
		// 		VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
		// 		Assets::Shader::Comp::presentMerge);
		//
		// 	presentDescriptorBuffer = DescriptorBuffer{
		// 			context.physicalDevice, context.device,
		// 		presentPipelineData.descriptorSetLayout,
		// 			presentPipelineData.descriptorSetLayout.size()
		// 		};
		//
		// 	gameUIMergeCommand = {context.device, commandPool_compute};
		//
		// 	finalStagingAttachment = Attachment{context.physicalDevice, context.device};
		// 	finalStagingAttachment.create(swapChain.size2D(), obtainTransientCommand(),
		// 	                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		// 	                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
		// 	                              VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
		// 	                              VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		// 	);
		//
		// 	updatePresentData();
		// }
		//
		// void updatePresentData(){
		// 	auto [ui1, ui2] = uiImageViewProv();
		//
		// 	VkSampler sampler = Assets::Sampler::blitSampler;
		// 	const VkDescriptorImageInfo worldInfo{
		// 		.sampler = sampler,
		// 		.imageView = rendererWorld->getResult_NFAA().getView(),
		// 		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		// 	};
		//
		// 	// const VkDescriptorImageInfo ui1Info{
		// 	// 	.sampler = sampler,
		// 	// 	.imageView = ui1,
		// 	// 	.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		// 	// };
		// 	//
		// 	// const VkDescriptorImageInfo ui2Info{
		// 	// 	.sampler = sampler,
		// 	// 	.imageView = ui2,
		// 	// 	.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		// 	// };
		//
		// 	presentDescriptorBuffer.load([&, this](const DescriptorBuffer& buffer){
		// 		buffer.loadImage(0, worldInfo);
		// 		// buffer.loadImage(1, ui1Info);
		// 		// buffer.loadImage(2, ui2Info);
		//
		// 		buffer.loadImage(
		// 			1, {
		// 				.sampler = nullptr,
		// 				.imageView = finalStagingAttachment.getView(),
		// 				.imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		// 			}, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		// 	});
		//
		// 	createPresentCommand();
		// }
		//
		// void createPresentCommand(){
		// 	const ScopedCommand scopedCommand{gameUIMergeCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};
		//
		// 	std::array barriers = {
		// 		VkImageMemoryBarrier2{
		// 			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		// 			.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		// 			.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
		// 			.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		// 			.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
		// 			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		// 			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		// 			.srcQueueFamilyIndex = context.graphicFamily(),
		// 			.dstQueueFamilyIndex = context.computeFamily(),
		// 			.image = finalStagingAttachment.getImage(),
		// 			.subresourceRange = ImageSubRange::Color
		// 		},
		// 	};
		//
		// 	Util::imageBarrier(scopedCommand, barriers);
		//
		// 	vkCmdBindPipeline(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE, presentPipelineData.pipeline);
		//
		// 	presentDescriptorBuffer.bindTo(
		// 		scopedCommand,
		// 		VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
		// 		VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
		// 	);
		//
		// 	EXT::cmdSetDescriptorBufferOffsetsEXT(
		// 		scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE,
		// 		presentPipelineData.layout,
		// 		0, 1, Seq::Indices<0>, Seq::Offset<0>);
		//
		// 	static constexpr Geom::USize2 UnitSize{16, 16};
		// 	const auto [ux, uy] = swapChain.size2D().add(UnitSize.copy().sub(1, 1)).div(UnitSize);
		//
		// 	vkCmdDispatch(scopedCommand, ux, uy, 1);
		//
		// 	Util::swapStage(barriers);
		// 	Util::imageBarrier(scopedCommand, barriers);
		// }
	};
}

module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Manager;

import std;

import ext.meta_programming;

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

		ext::EventManager eventManager{
			{ext::index_of<ResizeEvent>()}
		};

		std::function<std::pair<VkImageView, VkImageView>()> uiImageViewProv{};
		std::function<VkImageView()> worldImageViewProv{};

		Context context{};
		CommandPool commandPool{};
		CommandPool transientCommandPool{};

		Graphic::ComputePostProcessor presentMerge{};

		SwapChain swapChain{};
		RenderProcedure flushPass{};

		[[nodiscard]] TransientCommand obtainTransientCommand() const{
			return transientCommandPool.obtainTransient(context.device.getPrimaryGraphicsQueue());
		}

		struct InFlightData{
			Semaphore imageAvailableSemaphore{};
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

			presentMerge.submitCommand();

			const auto imageIndex = swapChain.acquireNextImage(currentFrameData.imageAvailableSemaphore);

			Util::submitCommand(
				context.device.getPrimaryGraphicsQueue(),
				swapChain.getCommandFlushes()[imageIndex],
				nullptr,
				currentFrameData.imageAvailableSemaphore.get(), VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				currentFrameData.flushFinishedSemaphore.get(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT
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

					port.in.insert_or_assign(0, worldImageViewProv());
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
				frameData.imageAvailableSemaphore = Semaphore{context.device};
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

				pipeline.builder = [](PipelineData& p){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.useDefaultFixedStages()
						.setColorBlend(&Default::ColorBlending<std::array{Blending::Disable}>)
						.setShaderChain({&Assets::Shader::Vert::blitSingle, &Assets::Shader::Frag::blitSingle})
						.setStaticViewportAndScissor(p.size.x, p.size.y);

					p.createPipeline(pipelineTemplate);
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
	};
}

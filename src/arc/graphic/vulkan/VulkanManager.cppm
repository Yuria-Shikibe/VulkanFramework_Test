module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Manager;

import std;

import Core.Window;

import Core.Vulkan.Context;
import Core.Vulkan.SwapChain;

import Core.Vulkan.Semaphore;

import Core.Vulkan.CommandPool;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Buffer.FrameBuffer;
import Core.Vulkan.Comp;
import Core.Vulkan.Fence;
import Core.Vulkan.RenderProcedure;
import Core.Vulkan.Attachment;

import Core.Vulkan.Util;

import Graphic.PostProcessor;

import Geom.Vector2D;

//TEMP
import Assets.Graphic;
import Assets.Graphic.PostProcess;

import ext.event;
import ext.circular_array;

export namespace Core::Vulkan{

	struct ResizeEvent final : Geom::USize2, ext::event_type{

		[[nodiscard]] explicit ResizeEvent(const Geom::USize2 size2) : Geom::USize2(size2){}
	};

	class VulkanManager{
	public:
		[[nodiscard]] VulkanManager() = default;

		ext::legacy_event_manager eventManager{
			{ext::index_of<ResizeEvent>()}
		};

		Graphic::AttachmentPort worldPort{};
		Graphic::AttachmentPort uiPort{};

		Context context{};

	private:
		CommandPool commandPool{};
		CommandPool transientCommandPool{};
		CommandPool commandPool_Compute{};

		Graphic::ComputePostProcessor presentMerge{};

		SwapChain swapChain{};
		RenderProcedure flushPass{};


		struct InFlightData{
			Fence fence{};
			Semaphore imageAvailableSemaphore{};
			Semaphore flushFinishedSemaphore{};
		};

		ext::circular_array<InFlightData, 4> frameDataArr{};

	public:
		template <std::regular_invocable<Geom::USize2> InitFunc>
		void registerResizeCallback(std::function<void(const ResizeEvent&)>&& callback, InitFunc initFunc) {
			initFunc(swapChain.size2D());
			eventManager.on<ResizeEvent>(std::move(callback));
		}

		void registerResizeCallback(std::function<void(const ResizeEvent&)>&& callback) {
			callback(ResizeEvent{swapChain.size2D()});
			eventManager.on<ResizeEvent>(std::move(callback));
		}

		void mergePresent() const{
			presentMerge.submitCommand();
		}

		void blitToScreen(){
			const auto& currentFrameData = ++frameDataArr;

			currentFrameData.fence.waitAndReset();

			const auto imageIndex = swapChain.acquireNextImage(currentFrameData.imageAvailableSemaphore);

			context.commandSubmit_Graphics(
				swapChain.getCommandFlushes()[imageIndex],
				currentFrameData.imageAvailableSemaphore,
				currentFrameData.flushFinishedSemaphore, currentFrameData.fence,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
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

			commandPool_Compute = CommandPool{
					context.device, context.computeFamily(),
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
				presentMerge = Assets::PostProcess::Factory::presentMerge.generate({swapChain.size2D(), [this]{
					Graphic::AttachmentPort port{};

					port.views.insert_or_assign(0, worldPort.views.at(0));
					port.views.insert_or_assign(1, uiPort.views.at(0));
					port.views.insert_or_assign(2, uiPort.views.at(1));

					return port;
				}, {}, commandPool_Compute.getTransient(context.device.getPrimaryComputeQueue()), commandPool_Compute.obtain()});
			}


		    updateFlushInputDescriptorSet();
			bindSwapChainFrameBuffer();

			swapChain.recreateCallback = [this](const SwapChain& swapChain){
				resize(swapChain);
			};

			createFlushCommands();

			std::cout.flush();
		}

		[[nodiscard]] TransientCommand obtainTransientCommand() const{
			return transientCommandPool.getTransient(context.device.getPrimaryGraphicsQueue());
		}

	private:
		void createFrameObjects(){
			for(auto& frameData : frameDataArr){
				frameData.fence = Fence{context.device, Fence::CreateFlags::signal};
				frameData.imageAvailableSemaphore = Semaphore{context.device};
				frameData.flushFinishedSemaphore = Semaphore{context.device};
			}
		}

        void resize(const SwapChain& swapChain){
			eventManager.fire(ResizeEvent{swapChain.size2D()});

			presentMerge.resize(swapChain.size2D(), commandPool_Compute.getTransient(context.device.getPrimaryComputeQueue()), commandPool_Compute.obtain());

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
				pipeline.createDescriptorLayout([this](DescriptorLayout& layout){
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

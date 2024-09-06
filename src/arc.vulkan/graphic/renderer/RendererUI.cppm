module;

#include <vulkan/vulkan.h>

export module Graphic.RendererUI;

import Core.Vulkan.BatchData;
import Core.Vulkan.Context;
import Core.Vulkan.RenderProcedure;
import Core.Vulkan.Buffer.FrameBuffer;
import Core.Vulkan.Attachment;
import Core.Vulkan.Uniform;
import Core.Vulkan.Fence;
import Core.Vulkan.Sampler;
import Core.Vulkan.Comp;
import Core.Vulkan.Image;
import Core.Vulkan.Vertex;
import Core.Vulkan.Semaphore;

import Core.Vulkan.Preinstall;
import Core.Vulkan.EXT;

import Graphic.Color;
import Graphic.Batch2;

import Geom.Rect_Orthogonal;

import std;

//TODO move this to other place
import Assets.Graphic;

export namespace Graphic{
	struct UniformUI{
		Core::Vulkan::UniformMatrix3D proj{};
	};

	struct RendererUI{
		//[base, def, light]
		static constexpr std::size_t AttachmentCount = 3;

		Batch2 batch{};

		using Batch = decltype(batch);

		Core::Vulkan::CommandPool drawCommandPool{};

		// std::array<Core::Vulkan::CommandBuffer, Batch::BufferSize> drawCommands{};
		Core::Vulkan::CommandBuffer batchDrawCommand{};
		Core::Vulkan::CommandBuffer mergeCommand{};
		Core::Vulkan::CommandBuffer blitEndCommand{};
		Core::Vulkan::CommandBuffer clearCommand{};

		Core::Vulkan::CommandBuffer scissorSetCommand{};

		// Core::Vulkan::BatchData batchData{};

		Core::Vulkan::RenderProcedure batchDrawPass{};
		Core::Vulkan::RenderProcedure mergeDrawPass{};

		Core::Vulkan::FramebufferLocal batchFramebuffer{};
		Core::Vulkan::FramebufferLocal mergeFrameBuffer{};

		// Core::Vulkan::Fence batchFence{};

		// std::uint32_t scissorDepth{};
		// Geom::OrthoRectUInt scissor{};
		std::vector<Geom::OrthoRectUInt> scissors{};

		[[nodiscard]] const Core::Vulkan::Context& context() const{ return *batch.context; }

		[[nodiscard]] RendererUI() = default;

		[[nodiscard]] explicit RendererUI(const Core::Vulkan::Context& context) :
			batch{context, sizeof(Core::Vulkan::Vertex_UI)},
			drawCommandPool{
				context.device, context.physicalDevice.queues.graphicsFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
			},
			batchDrawCommand{context.device, drawCommandPool},
			mergeCommand{context.device, drawCommandPool},
			blitEndCommand{context.device, drawCommandPool/*, VK_COMMAND_BUFFER_LEVEL_SECONDARY*/},
			clearCommand{context.device, drawCommandPool/*, VK_COMMAND_BUFFER_LEVEL_SECONDARY*/},
			scissorSetCommand{context.device, drawCommandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY}
			// batchFence{context.device, Core::Vulkan::Fence::CreateFlags::noSignal},
			/*batchData{batch.getBatchData()}*/{

			batch.cmdBinderBegin = [this](VkCommandBuffer scopedCommand, const VkDescriptorBufferBindingInfoEXT& imageInfo){
				using namespace Core::Vulkan;
				const auto renderPassInfo = batchDrawPass.getBeginInfo(batchFramebuffer);
				vkCmdBeginRenderPass(scopedCommand, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdSetScissor(scopedCommand, 0, 1, &reinterpret_cast<const VkRect2D&>(scissors.back()));

				const auto pipelineContext = batchDrawPass.startCmdContext(scopedCommand);

				const std::array infos{
					batchDrawPass.front().descriptorBuffers.front().getBindInfo(
						VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
						VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT),
					imageInfo
				};

				EXT::cmdBindDescriptorBuffersEXT(scopedCommand, infos.size(), infos.data());
				EXT::cmdSetDescriptorBufferOffsetsEXT(
					scopedCommand,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					batchDrawPass.front().layout,
					0, infos.size(), Seq::Indices<0, 1>, Seq::Offset<0>);

			};
		}

		Geom::USize2 size{};

		void resize(const Geom::USize2 size2){
			if(this->size == size2) return;

			vkQueueWaitIdle(context().device.getGraphicsQueue());

			this->size = size2;
			batchDrawPass.resize(size2);
			mergeDrawPass.resize(size2);

			auto cmd = batch.obtainTransientCommand();
			batchFramebuffer.resize(size2, cmd);

			for(std::size_t i = 0; i < AttachmentCount; ++i){
				mergeFrameBuffer.loadExternalImageView({{i, batchFramebuffer.atLocal(i).getView()}});
			}

			scissors.front() = {size.x, size.y};
			mergeFrameBuffer.resize(size2, cmd);
			cmd = {};

			setMergeDescriptorSet();
			createFlushCommands();
			createCommands();

			updateUniformBuffer();
		}

		Core::Vulkan::Attachment& mergedAttachment(){
			return mergeFrameBuffer.localAttachments.back();
		}

		void initRenderPass(const Geom::USize2 size2){
			this->size = size2;

			initPipeline();
			createFrameBuffers();

			setMergeDescriptorSet();
			createFlushCommands();
			createCommands();

			updateUniformBuffer();

			scissors.push_back({size.x, size.y});

			batchDrawPass.front().descriptorBuffers.front().load([this](Core::Vulkan::DescriptorBuffer& buffer){
				buffer.loadUniform(
				0, batchDrawPass.front().uniformBuffers.front().getBufferAddress(),
				batchDrawPass.front().uniformBuffers.front().requestedSize());
			});
		}

		void initPipeline(){
			using namespace Core::Vulkan;

			/*batchPass:*/
			{
				batchDrawPass.init(context());

				batchDrawPass.createRenderPass([](RenderPass& renderPass){
					const auto ColorAttachmentDesc = VkAttachmentDescription{}
						| AttachmentDesc::Default
						| AttachmentDesc::Stencil_DontCare
						| AttachmentDesc::ReusedColorAttachment
						| AttachmentDesc::ReadAndWrite;


					for(std::size_t i = 0; i < AttachmentCount; ++i){
						renderPass.pushAttachment(ColorAttachmentDesc);
					}

					renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
						for(std::size_t i = 0; i < AttachmentCount; ++i){
							subpass.addOutput(i);
						}
					});
				});

				batchDrawPass.pushAndInitPipeline([this](PipelineData& pipeline){
					pipeline.createDescriptorLayout([](DescriptorSetLayout& layout){
						layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
						layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
					});

					pipeline.createPipelineLayout(0, {batch.layout});

					// pipeline.createDescriptorSet(1);
					pipeline.createDescriptorBuffers(1);
					pipeline.addUniformBuffer(sizeof(UniformUI));

					pipeline.builder = [](PipelineData& pipeline){
						PipelineTemplate pipelineTemplate{};
						pipelineTemplate.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

						pipelineTemplate
							.useDefaultFixedStages()
							.setColorBlend(&Default::ColorBlending<std::array{
									Blending::AlphaBlend,
									Blending::AlphaBlend,
									Blending::AlphaBlend,
								}>)
							.setVertexInputInfo<Core::Vulkan::UIVertBindInfo>()
							.setShaderChain({&Assets::Shader::Vert::uiBatch, &Assets::Shader::Frag::uiBatch})
							.setStaticViewport(float(pipeline.size.x), float(pipeline.size.y))
							.setDynamicStates({VK_DYNAMIC_STATE_SCISSOR});

						pipeline.createPipeline(pipelineTemplate);
					};
				});

				batchDrawPass.resize(size);
			}

			/*mergePass:*/
			{
				mergeDrawPass.init(context());

				mergeDrawPass.createRenderPass([](RenderPass& renderPass){
					const auto ColorAttachmentDesc = VkAttachmentDescription{}
						| AttachmentDesc::Default
						| AttachmentDesc::Stencil_DontCare
						| AttachmentDesc::ReusedColorAttachment
						| AttachmentDesc::ReadAndWrite;

					for(std::size_t i = 0; i < AttachmentCount; ++i){
						renderPass.pushAttachment(ColorAttachmentDesc);
					}

					//out[base, light]
					renderPass.pushAttachment(ColorAttachmentDesc);
					renderPass.pushAttachment(ColorAttachmentDesc);

					renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
						subpass.addDependency(VkSubpassDependency{
								.dstSubpass = 0,
								.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
								.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
							} | Subpass::Dependency::External);

						for(std::size_t i = 0; i < AttachmentCount; ++i){
							subpass.addInput(i);
						}

						// subpass.addInput(AttachmentCount + 0);
						// subpass.addInput(AttachmentCount + 1);

						subpass.addOutput(AttachmentCount + 0);
						subpass.addOutput(AttachmentCount + 1);
					});
				});

				mergeDrawPass.pushAndInitPipeline([](PipelineData& pipeline){
					pipeline.createDescriptorLayout([](DescriptorSetLayout& layout){
						layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
						layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
						layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
						// layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
						// layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
					});

					pipeline.createPipelineLayout();
					pipeline.createDescriptorSet(1);

					pipeline.builder = [](PipelineData& pipeline){
						PipelineTemplate pipelineTemplate{};
						pipelineTemplate
							.useDefaultFixedStages()
							.setColorBlend(&Default::ColorBlending<std::array{
									Blending::ScaledAlphaBlend,
									Blending::AlphaBlend,
								}>)
							.setVertexInputInfo<Util::EmptyVertexBind>()
							.setShaderChain({&Assets::Shader::Vert::blitSingle, &Assets::Shader::Frag::uiMerge})
							.setStaticViewportAndScissor(pipeline.size.x, pipeline.size.y);

						pipeline.createPipeline(pipelineTemplate);
					};
				});

				mergeDrawPass.resize(size);
			}
		}

		void createFrameBuffers(){
			using namespace Core::Vulkan;

			batchFramebuffer = FramebufferLocal{context().device, size, batchDrawPass.renderPass};
			mergeFrameBuffer = FramebufferLocal{context().device, size, mergeDrawPass.renderPass};

			//TODO no sample
			for(std::size_t i = 0; i < AttachmentCount; ++i){
				ColorAttachment colorAttachment{context().physicalDevice, context().device};
				colorAttachment.create(size, batch.obtainTransientCommand(),
				                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
				                       VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
				);
				batchFramebuffer.pushCapturedAttachments(std::move(colorAttachment));
			}

			batchFramebuffer.loadCapturedAttachments(AttachmentCount);
			batchFramebuffer.create();

			for(std::size_t i = 0; i < 2; ++i){
				ColorAttachment colorAttachment{context().physicalDevice, context().device};
				colorAttachment.create(size, batch.obtainTransientCommand(),
				                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
				                       VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
				);
				mergeFrameBuffer.addCapturedAttachments(AttachmentCount + i, std::move(colorAttachment));
			}

			mergeFrameBuffer.loadCapturedAttachments(0);
			for(std::size_t i = 0; i < AttachmentCount; ++i){
				mergeFrameBuffer.loadExternalImageView({{i, batchFramebuffer.atLocal(i).getView()}});
			}
			mergeFrameBuffer.create();
		}

		void setMergeDescriptorSet(){
			using namespace Core::Vulkan;

			DescriptorSetUpdator updator{context().device, mergeDrawPass.atLocal(0).descriptorSets.front()};

			std::array layouts{
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
					// VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
					// VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
				};

			std::array<VkDescriptorImageInfo, std::ssize(layouts)> infos{};

			for(const auto& [index, attachment] : batchFramebuffer.localAttachments | std::views::enumerate){
				infos[index] = attachment.getDescriptorInfo(nullptr, layouts[index]);
			}

			// infos[AttachmentCount + 0] = mergeFrameBuffer.localAttachments.back().getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
			// infos[AttachmentCount + 1] = mergeFrameBuffer.localAttachments.back().getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);

			for(auto&& [index, inputAttachmentImageInfo] : infos | std::views::enumerate){
				updator.pushAttachment(inputAttachmentImageInfo);
			}

			updator.update();
		}

		void createFlushCommands(){
			using namespace Core::Vulkan;

			ScopedCommand scopedCommand{mergeCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};


			const auto renderPassInfo = mergeDrawPass.getBeginInfo(mergeFrameBuffer);
			vkCmdBeginRenderPass(scopedCommand, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			const auto pipelineContext = mergeDrawPass.startCmdContext(scopedCommand);

			pipelineContext.data().bindDescriptorTo(scopedCommand);
			scopedCommand->blitDraw();

			vkCmdEndRenderPass(scopedCommand);

			for(const auto& localAttachment : batchFramebuffer.localAttachments){
				localAttachment.cmdClearColor(scopedCommand, {}, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			}
		}

		void blit() const{
			context().commandSubmit_Graphics(mergeCommand, nullptr, nullptr);
		}

		void endBlit() const{
			context().commandSubmit_Graphics(blitEndCommand, nullptr, nullptr);
		}

		void clear() const{
			context().commandSubmit_Graphics(clearCommand, nullptr, nullptr);
		}

		void createCommands(){
			using namespace Core::Vulkan;

			{
				ScopedCommand scopedCommand{clearCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				for(const auto& localAttachment : mergeFrameBuffer.localAttachments){
					localAttachment.cmdClearColor(scopedCommand, {}, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
				}
			}

			{
				ScopedCommand scopedCommand{blitEndCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				for(const auto& attachment : mergeFrameBuffer.localAttachments){
					Util::transitionImageLayout(scopedCommand, attachment.getImage(), {
						                            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						                            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						                            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
						                            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
						                            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						                            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					                            }, VK_IMAGE_ASPECT_COLOR_BIT);
				}
			}
		}

		void updateUniformBuffer(){
			Geom::Matrix3D matrix3D{};
			matrix3D.setOrthogonal(0, 0, size.x, size.y);
			UniformUI uniformUi{matrix3D};
			batchDrawPass.front().uniformBuffers.front().memory.loadData(uniformUi);
		}

		void resetScissors(){
			const auto& current = scissors.back();
			const Geom::OrthoRectUInt next{size.x, size.y};

			if(current != next){
				scissors.clear();
				pushScissor(next, false);
			} else{
				scissors.clear();
				scissors.push_back(next);
			}
		}

		void pushScissor(Geom::OrthoRectUInt scissor, const bool useIntersection = true){
			if(useIntersection){
				const auto& last = scissors.back();
				scissor = scissor.intersectionWith(last);
			}
			scissors.push_back(scissor);

			batch.consumeAll();
		}

		void popScissor(){
#if DEBUG_CHECK
			if(scissors.size() <= 1){
				throw std::runtime_error{"Scissors underflow"};
			}

#endif

			scissors.pop_back();

			batch.consumeAll();
		}
	};
}

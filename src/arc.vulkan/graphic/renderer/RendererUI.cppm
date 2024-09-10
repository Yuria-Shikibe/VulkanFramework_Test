module;

#include <vulkan/vulkan.h>

export module Graphic.Renderer.UI;

export import Graphic.Renderer;

import Core.Vulkan.Context;
import Core.Vulkan.RenderProcedure;
import Core.Vulkan.Buffer.FrameBuffer;
import Core.Vulkan.Attachment;
import Core.Vulkan.Uniform;
import Core.Vulkan.Fence;
import Core.Vulkan.Sampler;
import Core.Vulkan.Comp;
import Core.Vulkan.Util;
import Core.Vulkan.Image;
import Core.Vulkan.Vertex;
import Core.Vulkan.Semaphore;

import Core.Vulkan.Preinstall;
import Core.Vulkan.EXT;

import Graphic.Batch;
import Graphic.PostProcessor;

import Geom.Rect_Orthogonal;

import std;
import ext.array_queue;
import ext.circular_array;

//TODO move this to other place
import Assets.Graphic;
import Assets.Graphic.PostProcess;

export namespace Graphic{
	struct UniformUI_Vertex{
		Core::Vulkan::UniformMatrix3D proj{};
	};

	struct UniformUI_Fragment{
		Geom::Rect_Orthogonal<float> scissor{};
	};

	struct RendererUI : BasicRenderer{
		//[base, def, light]
		static constexpr std::size_t AttachmentCount = 3;

		Batch batch{};

		using Batch = decltype(batch);

		struct ScissorStagingUnits{
			Core::Vulkan::StagingBuffer buffer{};
			Core::Vulkan::CommandBuffer transferCommand{};
			Core::Vulkan::Fence fence{};

			[[nodiscard]] ScissorStagingUnits() = default;

			[[nodiscard]] explicit ScissorStagingUnits(const RendererUI& rendererUi) :
				buffer{rendererUi.context().physicalDevice, rendererUi.context().device, sizeof(UniformUI_Fragment)},
				transferCommand{rendererUi.context().device, rendererUi.commandPool},
				fence{rendererUi.context().device, Core::Vulkan::Fence::CreateFlags::signal}
			{}
		};

		struct RenderCommandUnit{
			Core::Vulkan::CommandBuffer buffer{};

			[[nodiscard]] RenderCommandUnit() = default;

			[[nodiscard]] explicit RenderCommandUnit(const RendererUI& rendererUi) :
				buffer{rendererUi.context().device, rendererUi.commandPool}
			{}
		};

		ext::circular_array<ScissorStagingUnits, 3> scissorUnits{};

		std::array<RenderCommandUnit, Batch::UnitSize> drawCommands{};

		Core::Vulkan::CommandBuffer mergeCommand{};
		Core::Vulkan::CommandBuffer blitEndCommand{};
		Core::Vulkan::CommandBuffer clearCommand{};


		Core::Vulkan::RenderProcedure batchDrawPass{};

		Core::Vulkan::FramebufferLocal batchFramebuffer{};

		std::vector<Geom::OrthoRectUInt> scissors{};

		ComputePostProcessor mergeProcessor{};
		Core::Vulkan::CommandBuffer blittedClearCommand{};

		[[nodiscard]] const Core::Vulkan::Context& context() const noexcept{ return *batch.context; }

		[[nodiscard]] RendererUI() = default;

		[[nodiscard]] explicit RendererUI(const Core::Vulkan::Context& context) :
		BasicRenderer{context},
			batch{context, sizeof(Core::Vulkan::Vertex_UI), Assets::Sampler::textureDefaultSampler},

			mergeCommand{context.device, commandPool},
			blitEndCommand{context.device, commandPool},
			clearCommand{context.device, commandPool}
		{

			for (auto && element : drawCommands){
				element = std::decay_t<decltype(element)>{*this};
			}

			for (auto && element : scissorUnits){
				element = std::decay_t<decltype(element)>{*this};
			}

			batch.drawCall = [this](const std::size_t i){
				draw(i);
			};
		}

		void resize(const Geom::USize2 size2){
			if(this->size == size2) return;

			vkQueueWaitIdle(context().device.getPrimaryGraphicsQueue());

			this->size = size2;
			batchDrawPass.resize(size2);
			// mergeDrawPass.resize(size2);

			auto cmd = batch.obtainTransientCommand();
			batchFramebuffer.resize(size2, cmd);

			// for(std::size_t i = 0; i < AttachmentCount; ++i){
			// 	mergeFrameBuffer.loadExternalImageView({{i, batchFramebuffer.atLocal(i).getView()}});
			// }

			scissors.front() = {size.x, size.y};
			// mergeFrameBuffer.resize(size2, cmd);
			cmd = {};

			mergeProcessor.resize(size);

			createCommands();
			createDrawCommands();

			updateUniformBuffer();
		}

		void initRenderPass(const Geom::USize2 size2){
			this->size = size2;

			initPipeline();
			createFrameBuffers();

			{
				mergeProcessor = Assets::PostProcess::Factory::uiMerge.generate(size, [this]{
					AttachmentPort port{};

					for(const auto& [i, localAttachment] : batchFramebuffer.localAttachments | std::views::enumerate){
						port.addComputeInputAttachment(i, localAttachment);
					}

					return port;
				});
			}

			blittedClearCommand = mergeProcessor.commandPool.obtain();

			createCommands();

			updateUniformBuffer();

			scissors.push_back({size.x, size.y});

			batchDrawPass.front().descriptorBuffers.front().load([this](const Core::Vulkan::DescriptorBuffer& buffer){
				VkDeviceAddress address{getUBO().getBufferAddress()};
				buffer.loadUniform(0, address, sizeof(UniformUI_Vertex));
				address += sizeof(UniformUI_Vertex);
				buffer.loadUniform(1, address, sizeof(UniformUI_Fragment));
			});

			createDrawCommands();
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
						layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
						layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
					});

					pipeline.createPipelineLayout(0, {batch.layout});

					pipeline.createDescriptorBuffers(1);
					pipeline.addUniformBuffer(sizeof(UniformUI_Vertex) + sizeof(UniformUI_Fragment), VK_BUFFER_USAGE_TRANSFER_DST_BIT);

					pipeline.builder = [](PipelineData& pipeline){
						PipelineTemplate pipelineTemplate{};
						pipelineTemplate.info.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

						pipelineTemplate
							.useDefaultFixedStages()
							.setColorBlend(&Default::ColorBlending<std::array{
									Blending::ScaledAlphaBlend,
									Blending::AlphaBlend,
									Blending::AlphaBlend,
								}>)
							.setVertexInputInfo<Core::Vulkan::UIVertBindInfo>()
							.setShaderChain({&Assets::Shader::Vert::uiBatch, &Assets::Shader::Frag::uiBatch})
							.setStaticViewportAndScissor(pipeline.size.x, pipeline.size.y);
							// .setDynamicStates({VK_DYNAMIC_STATE_SCISSOR});

						pipeline.createPipeline(pipelineTemplate);
					};
				});

				batchDrawPass.resize(size);
			}
		}

		void createFrameBuffers(){
			using namespace Core::Vulkan;

			batchFramebuffer = FramebufferLocal{context().device, size, batchDrawPass.renderPass};

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

		}


		void draw(const std::size_t index) const{
			const auto& unit = drawCommands[index];
			context().commandSubmit_Graphics(unit.buffer, nullptr, nullptr);
		}

		void blit() const{
			mergeProcessor.submitCommand();
			clearBatch();
		}

		void endBlit() const{
			context().commandSubmit_Graphics(blitEndCommand, nullptr, nullptr);
		}

		void clearAll() const{
			clearBatch();

			Core::Vulkan::Util::submitCommand(context().device.getPrimaryComputeQueue(), blittedClearCommand);
		}
		void clearBatch() const{
			Core::Vulkan::Util::submitCommand(context().device.getPrimaryGraphicsQueue(), clearCommand);
		}

		void createCommands(){
			using namespace Core::Vulkan;

			{
				ScopedCommand scopedCommand{clearCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				for (auto && localAttachment : batchFramebuffer.localAttachments){
					localAttachment.cmdClearColor(scopedCommand, {});
				}

			}


			{
				ScopedCommand scopedCommand{blittedClearCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				std::array<VkImageMemoryBarrier2, 2> barriers{};

				for (auto && [i, barrier] : barriers | std::views::enumerate){
					barrier = VkImageMemoryBarrier2{
						.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.image = mergeProcessor.images[i].getImage(),
						.subresourceRange = ImageSubRange::Color
					}
					| MemoryBarrier2::Image::Default
					| MemoryBarrier2::Image::QueueLocal
					| MemoryBarrier2::Image::Src_ComputeRead
					| MemoryBarrier2::Image::Dst_TransferWrite;
				}

				Util::imageBarrier(scopedCommand, barriers);

				for (auto& image : mergeProcessor.images){
					image.getImage().cmdClearColor(scopedCommand, ImageSubRange::Color);
				}

				Util::swapStage(barriers);
				Util::imageBarrier(scopedCommand, barriers);
			}

			for (auto& scissorUnit : scissorUnits){
				const ScopedCommand scopedCommand{scissorUnit.transferCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				std::array<VkBufferMemoryBarrier2, 1> barriers{};

				barriers[0] = VkBufferMemoryBarrier2{
					.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					.srcAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
					.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
					.buffer = getUBO(),
					.offset = sizeof(UniformUI_Vertex),
					.size = sizeof(UniformUI_Fragment)
				} | MemoryBarrier2::Buffer::Default | MemoryBarrier2::Buffer::QueueLocal;

				VkDependencyInfo dependencyInfo2{
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.bufferMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size()),
					.pBufferMemoryBarriers = barriers.data(),
				};

				vkCmdPipelineBarrier2(scopedCommand, &dependencyInfo2);

				scissorUnit.buffer.copyBuffer(scopedCommand, getUBO(), {
					.srcOffset = 0,
					.dstOffset = sizeof(UniformUI_Vertex),
					.size = sizeof(UniformUI_Fragment),
				});

				for (auto && barrier : barriers){
					Util::swapStage(barrier);
				}

				vkCmdPipelineBarrier2(scopedCommand, &dependencyInfo2);
			}

		}

		void createDrawCommands(){
			for(auto&& [i, unit] : drawCommands | std::views::enumerate){
				using namespace Core::Vulkan;
				auto& commandUnit = batch.units[i];

				auto barriers =
					batch.getBarriars(commandUnit.vertexBuffer, commandUnit.indirectBuffer.getTargetBuffer(), commandUnit.descriptorBuffer_Double);

				VkDependencyInfo dependencyInfo{
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.bufferMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size()),
					.pBufferMemoryBarriers = barriers.data(),
				};

				ScopedCommand scopedCommand{
					unit.buffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
				};

				vkCmdPipelineBarrier2(scopedCommand, &dependencyInfo);

				const auto renderPassInfo = batchDrawPass.getBeginInfo(batchFramebuffer);
				vkCmdBeginRenderPass(scopedCommand, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				const auto pipelineContext = batchDrawPass.startCmdContext(scopedCommand);

				const std::array infos{
						batchDrawPass.front().descriptorBuffers.front().getBindInfo(
							VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT),
						commandUnit.getDescriptorBufferBindInfo()
					};

				EXT::cmdBindDescriptorBuffersEXT(scopedCommand, infos.size(), infos.data());
				EXT::cmdSetDescriptorBufferOffsetsEXT(
					scopedCommand,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					batchDrawPass.front().layout,
					0, infos.size(), Seq::Indices<0, 1>, Seq::Offset<0, 0>);

				batch.bindBuffersTo(scopedCommand, i);

				vkCmdEndRenderPass(scopedCommand);

				for (auto && barrier : barriers){
					Util::swapStage(barrier);
				}

				vkCmdPipelineBarrier2(scopedCommand, &dependencyInfo);
			}
		}

		void updateUniformBuffer(){
			auto& ubo = getUBO();
			Geom::Matrix3D matrix3D{};
			matrix3D.setOrthogonal(0, 0, size.x, size.y);
			UniformUI_Vertex uniformUiVert{matrix3D};
			UniformUI_Fragment uniformUiFrag{{-1., -1., 1., 1.}};

			ubo.memory.loadData(uniformUiVert, 0);
			ubo.memory.loadData(uniformUiFrag, sizeof(UniformUI_Vertex));
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
			setScissor(scissors.back());
		}

		auto& getUBO(){
			return batchDrawPass.front().uniformBuffers.front();
		}

		void popScissor(){
#if DEBUG_CHECK
			if(scissors.size() <= 1){
				throw std::runtime_error{"Scissors underflow"};
			}
#endif

			scissors.pop_back();
			batch.consumeAll();
			setScissor(scissors.back());
		}

	private:
		void setScissor(const Geom::OrthoRectUInt& rectFloat){
			auto r = rectFloat.as<float>();
			auto [x, y] = size.as<float>();
			r.scl(2 / x, 2 / y);
			r.move(-1, -1);

			const UniformUI_Fragment uniformUiFrag{{r.getSrcX(), r.getSrcY(), r.getEndX(), r.getEndY()}};

			const auto& cur = ++scissorUnits;
			cur.buffer.memory.loadData(uniformUiFrag);
			cur.fence.waitAndReset();
			context().commandSubmit_Graphics(cur.transferCommand, nullptr, nullptr, cur.fence);
		}
	};
}

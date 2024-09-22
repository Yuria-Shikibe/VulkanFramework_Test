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
import Core.Vulkan.Event;
import Core.Vulkan.DynamicRendering;

import Core.Vulkan.Preinstall;
import Core.Vulkan.EXT;

import Graphic.Batch;
import Graphic.Batch.Exclusive;

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
		std::array<float, 4> scissor{};
	};

	struct RendererUI : BasicRenderer{
		//[base, def, light]
		static constexpr std::size_t AttachmentCount = 3;

		Batch_Exclusive batch{};
		using Batch = decltype(batch);

		//Pipeline Data
		Core::Vulkan::DynamicRendering dynamicRendering{};
		Core::Vulkan::SinglePipelineData pipelineData{};

		//Descriptor Region
		Core::Vulkan::DescriptorLayout scissorDescriptorLayout{};
		Core::Vulkan::UniformBuffer projUniformBuffer{};
		Core::Vulkan::DescriptorBuffer projDescriptorBuffer{};

		struct FrameData{

			//Descriptor Region
			Core::Vulkan::StagingBuffer scissorStagingBuffer{};
			Core::Vulkan::Event event{};
			Core::Vulkan::ExclusiveBuffer uniformBuffer{};
			Core::Vulkan::DescriptorBuffer descriptorBuffer{};
			Geom::OrthoRectFloat scissor{};

			[[nodiscard]] VkBufferMemoryBarrier2 getBarrier() const{
				return VkBufferMemoryBarrier2{
					.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					.srcAccessMask = VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT,
					.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
					.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
					.buffer = uniformBuffer,
					.offset = 0,
					.size = sizeof(UniformUI_Fragment),
				} | Core::Vulkan::MemoryBarrier2::Buffer::Default | Core::Vulkan::MemoryBarrier2::Buffer::QueueLocal;
			}

			[[nodiscard]] FrameData() = default;

			[[nodiscard]] explicit FrameData(
				const RendererUI& rendererUi, const Core::Vulkan::DescriptorLayout& layout) :
				scissorStagingBuffer{
					rendererUi.context().physicalDevice, rendererUi.context().device, sizeof(UniformUI_Fragment)
				},
				event{rendererUi.context().device, false},
				uniformBuffer{
					rendererUi.context().physicalDevice,
					rendererUi.context().device,
					sizeof(UniformUI_Vertex),
					VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
					VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
				},
				descriptorBuffer{rendererUi.context().physicalDevice, rendererUi.context().device, layout, layout.size()}{
				uniformBuffer.setAddress();
				descriptorBuffer.load([this](const Core::Vulkan::DescriptorBuffer& buffer){
					VkDeviceAddress address{uniformBuffer.getBufferAddress()};
					buffer.loadUniform(0, address, sizeof(UniformUI_Fragment));
				});
				event.set();
			}

			void waitAndReset() const{
				while(!event.isSet()){
					//TODO outoftime check?
				}
			}



		};

		Batch::DrawCommandSeq<FrameData> drawCommands{};
		std::size_t nextIndex{};

		Core::Vulkan::CommandBuffer mergeCommand{};
		Core::Vulkan::CommandBuffer blitEndCommand{};
		Core::Vulkan::CommandBuffer clearCommand{};

		std::array<Core::Vulkan::ColorAttachment, 3> attachments{};

		std::vector<Geom::OrthoRectFloat> scissors{};


		//post processors
		ComputePostProcessor mergeProcessor{};
		Core::Vulkan::CommandBuffer mergeAttachmentClearCommand{};

		[[nodiscard]] const Core::Vulkan::Context& context() const noexcept{ return *batch.context; }

		[[nodiscard]] RendererUI() = default;

		[[nodiscard]] explicit RendererUI(const Core::Vulkan::Context& context) :
		BasicRenderer{context},
			batch{context, sizeof(Core::Vulkan::Vertex_UI), Assets::Sampler::textureDefaultSampler},
			pipelineData{&context},
			projUniformBuffer{
				context.physicalDevice, context.device,
				sizeof(UniformUI_Vertex)},

			mergeCommand{context.device, commandPool},
			blitEndCommand{context.device, commandPool},
			clearCommand{context.device, commandPool}
		{
			using namespace Core::Vulkan;

			for (auto& attachment : attachments){
				attachment = {context.physicalDevice, context.device};
			}

			scissorDescriptorLayout = DescriptorLayout{context.device, [](DescriptorLayout& layout){
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
				layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
			}};

			pipelineData.createDescriptorLayout([](DescriptorLayout& layout){
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
				layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
			});

			pipelineData.createPipelineLayout(0, {scissorDescriptorLayout, batch.layout});

			projDescriptorBuffer = DescriptorBuffer{
				this->context().physicalDevice,
				this->context().device,
				pipelineData.descriptorSetLayout, pipelineData.descriptorSetLayout.size()};

			projDescriptorBuffer.load([this](const DescriptorBuffer& buffer){
				buffer.loadUniform(0, projUniformBuffer.getBufferAddress(), sizeof(UniformUI_Vertex));
			});

			for (auto && element : drawCommands){
				element = {commandPool.obtain(), FrameData{*this, scissorDescriptorLayout}};
			}

			batch.externalDrawCall = [this](const Batch::CommandUnit& unit, const std::size_t i){
				draw(unit, i);
			};
		}

		void resize(const Geom::USize2 size2){
			if(this->size == size2) return;

			vkQueueWaitIdle(context().device.getPrimaryGraphicsQueue());
			resetCommandPool();

			size = size2;
			createPipeline();

			auto cmd = batch.obtainTransientCommand();
			for (auto && attachment : attachments){
				attachment.resize(size2, cmd);
			}
			cmd = {};

			scissors.front() = Geom::OrthoRectFloat{size.as<float>()};

			mergeProcessor.resize(size, commandPool_Compute.getTransient(context().device.getPrimaryComputeQueue()), commandPool_Compute.obtain());

			createCommands();
			createDrawCommands();

			updateProjection();
			setPort();
		}

		void init(const Geom::USize2 size2){
			this->size = size2;

			createPipeline();
			createAttachments();

			{
				mergeProcessor = Assets::PostProcess::Factory::uiMerge.generate({size, [this]{
					AttachmentPort port{};

					for(const auto& [i, localAttachment] : attachments | std::views::enumerate){
						port.addAttachment(i, localAttachment);
					}

					return port;
				}, {}, commandPool_Compute.getTransient(context().device.getPrimaryComputeQueue()), commandPool_Compute.obtain()});
			}

			mergeAttachmentClearCommand = commandPool_Compute.obtain();

			createCommands();

			updateProjection();

			scissors.push_back(Geom::OrthoRectFloat{size.as<float>()});

			createDrawCommands();
			setPort();
		}

		void draw(const Batch::CommandUnit& unit, const std::size_t index){
			Core::Vulkan::Util::submitCommand(
							context().device.getPrimaryGraphicsQueue(),
							unit.transferCommand.get(), unit.fence);


			if(drawCommands[index].second.scissor != getCurrentScissor()){
				setScissor(getCurrentScissor());
			}

			Core::Vulkan::Util::submitCommand(
				context().device.getPrimaryGraphicsQueue(),
				drawCommands[index].first.get(), nullptr);

			nextIndex = (index + 1) % drawCommands.size();
		}

		void blit() const{
			Core::Vulkan::Util::submitCommand(context().device.getPrimaryComputeQueue(),
			std::array{mergeProcessor.mainCommandBuffer.get(), clearCommand.get()});
		}

		void endBlit() const{
			context().commandSubmit_Graphics(blitEndCommand, nullptr, nullptr);
		}

		void clearMerged() const{
			Core::Vulkan::Util::submitCommand(context().device.getPrimaryComputeQueue(), mergeAttachmentClearCommand);
		}

		void resetScissors(){
			const auto& current = scissors.back();
			const Geom::OrthoRectFloat next{size.as<float>()};

			if(current != next){
				scissors.clear();
				pushScissor(next, false);
			} else{
				scissors.clear();
				scissors.push_back(next);
			}
		}

		void pushScissor(Geom::OrthoRectFloat scissor, const bool useIntersection = true){
			if(useIntersection){
				const auto& last = scissors.back();
				scissor = scissor.intersectionWith(last);
			}

			batch.consumeAll();
			scissors.push_back(scissor);
			// setScissor(scissors.back());
		}

		[[nodiscard]] const Geom::OrthoRectFloat& getCurrentScissor() const{
			return scissors.back();
		}

		void popScissor(){
#if DEBUG_CHECK
			if(scissors.size() <= 1){
				throw std::runtime_error{"Scissors underflow"};
			}
#endif

			batch.consumeAll();
			scissors.pop_back();
			// setScissor(scissors.back());
		}

	private:
		void createAttachments(){
			using namespace Core::Vulkan;

			for (auto && attachment : attachments){
				attachment.create(size, batch.obtainTransientCommand(),
				   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
				);
			}
		}

		void createPipeline(){
			using namespace Core::Vulkan;

			DynamicPipelineTemplate pipelineTemplate{};
			pipelineTemplate.info.flags =
				VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

			pipelineTemplate
				.useDefaultFixedStages()
				.setColorBlend(&Default::ColorBlending<std::array{
						Blending::ScaledAlphaBlend,
						Blending::ScaledAlphaBlend,
						Blending::AlphaBlend,
					}>)
				.setVertexInputInfo<UIVertBindInfo>()
				.setShaderChain({&Assets::Shader::Vert::uiBatch, &Assets::Shader::Frag::uiBatch})
				.setStaticViewportAndScissor(size.x, size.y);

			pipelineTemplate.pushColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM, 3);
			pipelineTemplate.applyDynamicRendering();

			pipelineData.createPipeline(pipelineTemplate);
		}

		void createCommands(){
			using namespace Core::Vulkan;

			{
				ScopedCommand scopedCommand{clearCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				std::array<VkImageMemoryBarrier2, AttachmentCount> barriers{};

				for(const auto& [i, barrier] : barriers | std::views::enumerate){
					barrier =
						VkImageMemoryBarrier2{
							.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
							.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							.image = attachments[i].getImage(),
							.subresourceRange = ImageSubRange::Color
						}
					| MemoryBarrier2::Image::Default
					| MemoryBarrier2::Image::QueueLocal
					| MemoryBarrier2::Image::Src_ComputeRead
					| MemoryBarrier2::Image::Dst_TransferWrite;
				}

				Util::imageBarrier(scopedCommand, barriers);

				for (auto& attachment : attachments){
					attachment.getImage().cmdClearColor(scopedCommand, ImageSubRange::Color, {});
				}

				Util::swapStage(barriers);

				for (auto& b : barriers){
					b.srcQueueFamilyIndex = context().computeFamily();
					b.dstQueueFamilyIndex = context().graphicFamily();
					b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					b.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
					b.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
				}

				Util::imageBarrier(scopedCommand, barriers);
			}

			{
				ScopedCommand scopedCommand{mergeAttachmentClearCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

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

			// for (auto& scissorUnit : scissorUnits){
			// 	const ScopedCommand scopedCommand{scissorUnit.transferCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};
			//
			// 	std::array<VkBufferMemoryBarrier2, 2> barriers{};

				// barriers[0] = VkBufferMemoryBarrier2{
				// 	.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				// 	.srcAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
				// 	.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				// 	.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				// 	.buffer = getUBO(),
				// 	.offset = sizeof(UniformUI_Vertex),
				// 	.size = sizeof(UniformUI_Fragment)
				// } | MemoryBarrier2::Buffer::Default | MemoryBarrier2::Buffer::QueueLocal;
				//
				// barriers[1] = VkBufferMemoryBarrier2{
				// 	.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
				// 	.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
				// 	.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				// 	.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
				// 	.buffer = scissorUnit.buffer,
				// 	.offset = 0,
				// 	.size = VK_WHOLE_SIZE
				// } | MemoryBarrier2::Buffer::Default | MemoryBarrier2::Buffer::QueueLocal;

				// Util::bufferBarrier(scopedCommand, barriers);

				// scissorUnit.buffer.copyBuffer(scopedCommand, getUBO(), {
				// 	.srcOffset = 0,
				// 	.dstOffset = sizeof(UniformUI_Vertex),
				// 	.size = sizeof(UniformUI_Fragment),
				// });

				// Util::swapStage(barriers);
				// Util::bufferBarrier(scopedCommand, barriers);
			// }

		}

		void createDrawCommands(){
			dynamicRendering.clearColorAttachments();
			for (const auto & localAttachment : attachments){
				dynamicRendering.pushColorAttachment(localAttachment.getView(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}


			for(auto&& [i, unit] : drawCommands | std::views::enumerate){
				using namespace Core::Vulkan;
				auto& commandUnit = batch.units[i];

				ScopedCommand scopedCommand{
					unit.first, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
				};

				auto uboBarrier = unit.second.getBarrier();

				unit.second.event.cmdReset(scopedCommand,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
				// Util::bufferBarrier(scopedCommand, std::array{uboBarrier});
				unit.second.scissorStagingBuffer.copyBuffer(scopedCommand, unit.second.uniformBuffer, sizeof(UniformUI_Fragment));

				Util::swapStage(uboBarrier);
				Util::bufferBarrier(scopedCommand, commandUnit.getBarriers());
				// unit.second.event.cmdSet(scopedCommand, Util::dependencyOf(std::array{uboBarrier}));

				{
					dynamicRendering.beginRendering(scopedCommand, {{}, {size.x, size.y}});
					pipelineData.bind(scopedCommand, VK_PIPELINE_BIND_POINT_GRAPHICS);

					const std::array infos{
						projDescriptorBuffer.getBindInfo(
							VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT),
						unit.second.descriptorBuffer.getBindInfo(
							VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT),
						commandUnit.getDescriptorBufferBindInfo(),
					};

					EXT::cmdBindDescriptorBuffersEXT(scopedCommand, infos.size(), infos.data());
					EXT::cmdSetDescriptorBufferOffsetsEXT(
						scopedCommand,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						pipelineData.layout,
						0, infos.size(), Seq::Indices<0, 1, 2>, Seq::Offset<0, 0, 0>);


					batch.bindBuffersTo(scopedCommand, i);

					vkCmdEndRendering(scopedCommand);
				}

				// Util::swapStage(barriers);

				// Util::bufferBarrier(scopedCommand, barriers);
			}
		}

		void updateProjection() const{
			const auto& ubo = projUniformBuffer;
			Geom::Matrix3D matrix3D{};
			matrix3D.setOrthogonal(0, 0, size.x, size.y);
			UniformUI_Vertex uniformUiVert{matrix3D};

			ubo.memory.loadData(uniformUiVert, 0);
		}

		// auto& getUBO(){
		// 	return uiUniformBuffer;
		// }

		void setScissor(const Geom::OrthoRectFloat r){
			auto [x, y] = size.as<float>();

			x = 2 / x;
			y = 2 / y;

			const UniformUI_Fragment uniformUiFrag{
				{
					r.getSrcX() * x - 1, 1 - r.getEndY() * y,
					r.getEndX() * x - 1, 1 - r.getSrcY() * y
				}};

			auto& nextUnit = drawCommands[nextIndex].second;
			nextUnit.scissor = r;
			// nextUnit.waitAndReset();
			nextUnit.scissorStagingBuffer.memory.loadData(uniformUiFrag);
		}

		void setPort(){
			for (const auto & [i, attachment] : mergeProcessor.images | std::views::enumerate){
				port.addAttachment(i, attachment);
			}

		}
	};
}

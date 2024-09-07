module;

#include <vulkan/vulkan.h>

export module Graphic.Renderer.World;

export import Graphic.Renderer;
export import Graphic.Batch2;

import Core.Vulkan.RenderProcedure;
import Core.Vulkan.DynamicRendering;
import Core.Vulkan.Vertex;
import Core.Vulkan.Preinstall;
import Core.Vulkan.Util;
import Core.Vulkan.Image;
import Core.Vulkan.Uniform;

import Core.Vulkan.Attachment;

import Graphic.PostProcessor;

import std;

//TODO TEMP
import Assets.Graphic;
import Assets.Graphic.PostProcess;

export namespace Graphic{
	struct CameraProperties{
		float scale{};
	};

	struct RendererWorld : BasicRenderer{
		Batch2 batch{};
		using Batch = decltype(batch);

		//Pipeline Data
		Core::Vulkan::DynamicRendering dynamicRendering{};
		Core::Vulkan::SinglePipelineData pipelineData{};


		//Commands Region
		std::array<Core::Vulkan::CommandBuffer, Batch::UnitSize> drawCommands{};
		Core::Vulkan::CommandBuffer flushTransferCommand{};


		//Descriptor Region
		Core::Vulkan::UniformBuffer worldUniformBuffer{};
		Core::Vulkan::DescriptorBuffer worldBatchDescriptorBuffer{};

		Core::Vulkan::UniformBuffer cameraPropertiesUniformBuffer{};
		Core::Vulkan::DescriptorSetLayout cameraPropertyDescriptorLayout{};
		Core::Vulkan::DescriptorBuffer cameraPropertiesDescriptor{};


		//Attachments
		Core::Vulkan::ColorAttachment baseColor{};
		Core::Vulkan::ColorAttachment lightColor{};
		Core::Vulkan::DepthStencilAttachment depthStencilAttachment{};

		Core::Vulkan::ImageView depthAttachmentView{};


		//Post Processors
		ComputePostProcessor gussian{};
		ComputePostProcessor ssao{};


		[[nodiscard]] RendererWorld() = default;

		[[nodiscard]] explicit RendererWorld(const Core::Vulkan::Context& context)
			: BasicRenderer(context),
			  batch(context, sizeof(Core::Vulkan::Vertex_World), Assets::Sampler::textureNearestSampler),
			  pipelineData{&context},
			  worldUniformBuffer{context.physicalDevice, context.device, sizeof(Core::Vulkan::UniformProjectionBlock)},
			  cameraPropertiesUniformBuffer{context.physicalDevice, context.device, sizeof(CameraProperties)},
			  cameraPropertyDescriptorLayout{
				  context.device, [](Core::Vulkan::DescriptorSetLayout& layout){
					  layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
					  layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
				  }
			  },
			  cameraPropertiesDescriptor{
				  context.physicalDevice, context.device, cameraPropertyDescriptorLayout, cameraPropertyDescriptorLayout.size()
			  }{
			for(auto&& vkCommandBuffer : drawCommands){
				vkCommandBuffer = {context.device, commandPool};
			}

			batch.drawCall = [this](const std::size_t idx){
				this->context().commandSubmit_Graphics(drawCommands[idx], nullptr, nullptr);
			};


			using namespace Core::Vulkan;
			pipelineData.createDescriptorLayout([](DescriptorSetLayout& layout){
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
				layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
			});

			pipelineData.createPipelineLayout(0, {batch.layout});

			worldBatchDescriptorBuffer = DescriptorBuffer{this->context().physicalDevice, this->context().device, pipelineData.descriptorSetLayout, pipelineData.descriptorSetLayout.size()};
			worldBatchDescriptorBuffer.load([this](const DescriptorBuffer& descriptorBuffer){
				descriptorBuffer.loadUniform(0, worldUniformBuffer.getBufferAddress(), worldUniformBuffer.requestedSize());
			});

			cameraPropertiesDescriptor.load([this](const DescriptorBuffer& descriptorBuffer){
				descriptorBuffer.loadUniform(0, cameraPropertiesUniformBuffer.getBufferAddress(), cameraPropertiesUniformBuffer.requestedSize());
			});
		}

		void initPipeline(const Geom::USize2 size2){
			this->size = size2;

			baseColor = {context().physicalDevice, context().device};
			lightColor = {context().physicalDevice, context().device};
			depthStencilAttachment = {context().physicalDevice, context().device};

			{
				const auto cmd = batch.obtainTransientCommand();

				baseColor.create(
					size, cmd,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT |
					VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
					VK_IMAGE_USAGE_SAMPLED_BIT);

				lightColor.create(
					size, cmd,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT |
					VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
					VK_IMAGE_USAGE_SAMPLED_BIT);

				depthStencilAttachment.create(
					size, cmd,
					VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
				);
			}

			createDepthView();
			createPostProcessors();

			createPipeline();
			createDrawCommands();
		}

		decltype(auto) context() const {
			return *batch.context;
		}

		void updateProjection(const Core::Vulkan::UniformProjectionBlock& data) const{
			worldUniformBuffer.memory.loadData(data);
		}
		void updateCameraProperties(const CameraProperties& data) const{
			cameraPropertiesUniformBuffer.memory.loadData(data);
		}

		void resize(const Geom::USize2 size2){
			this->size = size2;
			vkQueueWaitIdle(context().device.getGraphicsQueue());

			{
				const auto cmd = batch.obtainTransientCommand();

				baseColor.resize(size2, cmd);
				lightColor.resize(size2, cmd);
				depthStencilAttachment.resize(size2, cmd);
			}
			createDepthView();

			gussian.resize(size);

			createPipeline();
			createDrawCommands();
		}

		Core::Vulkan::Attachment& getGaussianResult(){
			return gussian.images.back();
		}

		void post_gaussian() const{
			gussian.submitCommand();
		}

	private:
		void createPostProcessors(){
			{
				gussian = Assets::PostProcess::Factory::gaussianFactory.generate(size, [this]{
					AttachmentPort port{};

					port.in.insert_or_assign(0, lightColor.getView());
					port.toTransferOwnership.insert_or_assign(0, lightColor.getImage());
					return port;
				}, [this]{
					return std::vector{cameraPropertyDescriptorLayout.get()};
				});

				gussian.commandRecorderAdditional = [this](VkCommandBuffer scopedCommand){
					cameraPropertiesDescriptor.bindTo(scopedCommand,
						VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR|VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
					);

					Core::Vulkan::EXT::cmdSetDescriptorBufferOffsetsEXT(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE, gussian.pipelineData.layout,
						1, 1, Core::Vulkan::Seq::Indices<0>, Core::Vulkan::Seq::Offset<0>);
				};

				gussian.updateDescriptors();
				gussian.recordCommand();
			}

			{
				ssao = Assets::PostProcess::Factory::ssaoFactory.generate(size, [this]{
					AttachmentPort port{};

					port.in.insert_or_assign(0, depthAttachmentView);
					port.toTransferOwnership.insert_or_assign(0, depthStencilAttachment.getImage());
					return port;
				}, [this]{
					return std::vector{cameraPropertyDescriptorLayout.get()};
				});

				ssao.commandRecorderAdditional = [this](VkCommandBuffer scopedCommand){
					cameraPropertiesDescriptor.bindTo(scopedCommand,
						VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR|VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
					);

					Core::Vulkan::EXT::cmdSetDescriptorBufferOffsetsEXT(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE, gussian.pipelineData.layout,
						1, 1, Core::Vulkan::Seq::Indices<0>, Core::Vulkan::Seq::Offset<0>);
				};

				ssao.updateDescriptors();
				ssao.recordCommand();
			}
		}

		void createDepthView() {
			depthAttachmentView = Core::Vulkan::ImageView{context().device, {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.image = depthStencilAttachment.getImage(),
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

		void createDrawCommands(){
			dynamicRendering.clearColorAttachments();
			dynamicRendering.setDepthAttachment(depthStencilAttachment.getView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
			dynamicRendering.pushColorAttachment(baseColor.getView(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			dynamicRendering.pushColorAttachment(lightColor.getView(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

			for(auto&& [i, unit] : drawCommands | std::views::enumerate){
				using namespace Core::Vulkan;
				auto& commandUnit = batch.units[i];

				auto barriers = commandUnit.getBarriers();

				VkDependencyInfo dependencyInfo{
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.bufferMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size()),
					.pBufferMemoryBarriers = barriers.data(),
				};

				ScopedCommand scopedCommand{unit, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				vkCmdPipelineBarrier2(scopedCommand, &dependencyInfo);

				dynamicRendering.beginRendering(scopedCommand, {{}, {size.x, size.y}});

				vkCmdBindPipeline(scopedCommand, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.pipeline);

				const std::array infos{
					worldBatchDescriptorBuffer.getBindInfo(
						VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT),
					commandUnit.getDescriptorBufferBindInfo()
				};

				EXT::cmdBindDescriptorBuffersEXT(scopedCommand, infos.size(), infos.data());
				EXT::cmdSetDescriptorBufferOffsetsEXT(
					scopedCommand,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					pipelineData.layout,
					0, infos.size(), Seq::Indices<0, 1>, Seq::Offset<0, 0>);

				batch.bindBuffersTo(scopedCommand, i);

				vkCmdEndRendering(scopedCommand);

				for (auto && barrier : barriers){
					Util::swapStage(barrier);
				}

				vkCmdPipelineBarrier2(scopedCommand, &dependencyInfo);
			}
		}

		void createPipeline(){
			using namespace Core::Vulkan;

			DynamicPipelineTemplate pipelineTemplate{};
			pipelineTemplate.info.flags =
				VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

			pipelineTemplate
				.useDefaultFixedStages()
				.setDepthStencilState(Default::DefaultDepthStencilState)
				.setColorBlend(&Default::ColorBlending<std::array{
						Blending::AlphaBlend,
						Blending::ScaledAlphaBlend,
						// Blending::AlphaBlend,
					}>)
				.setVertexInputInfo<WorldVertBindInfo>()
				.setShaderChain({&Assets::Shader::Vert::worldBatch, &Assets::Shader::Frag::worldBatch})
				.setStaticViewportAndScissor(size.x, size.y);

			pipelineTemplate.pushColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM, 2);
			pipelineTemplate.applyDynamicRendering();

			pipelineData.createPipeline(pipelineTemplate);
		}
	};
}
module;

#include <vulkan/vulkan.h>

export module Graphic.Renderer.World;

export import Graphic.Renderer;
export import Graphic.Batch;

import Core.Vulkan.RenderProcedure;
import Core.Vulkan.DynamicRendering;
import Core.Vulkan.Vertex;
import Core.Vulkan.Preinstall;
import Core.Vulkan.Util;
import Core.Vulkan.Comp;
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
		Batch batch{};

		//Pipeline Data
		Core::Vulkan::DynamicRendering dynamicRendering{};
		Core::Vulkan::SinglePipelineData pipelineData{};


		//Commands Region
		Batch::DrawCommandSeq drawCommands{};
		Core::Vulkan::CommandBuffer cleanCommand{};
		Core::Vulkan::CommandBuffer endBatchCommand{};


		//Descriptor Region
		Core::Vulkan::UniformBuffer worldUniformBuffer{};
		Core::Vulkan::DescriptorBuffer descriptorBuffer{};

		Core::Vulkan::UniformBuffer cameraPropertiesUniformBuffer{};
		Core::Vulkan::DescriptorSetLayout cameraPropertyDescriptorLayout{};
		Core::Vulkan::DescriptorBuffer cameraPropertiesDescriptor{};

		//Attachments
		Core::Vulkan::ColorAttachment baseColor{};
		Core::Vulkan::ColorAttachment lightColor{};
		Core::Vulkan::DepthStencilAttachment depthStencilAttachment{};

		Core::Vulkan::ImageView depthAttachmentView{};


		//Post Processors
		ComputePostProcessor gaussian{};
		ComputePostProcessor ssao{};
		ComputePostProcessor merge{};
		ComputePostProcessor nfaa{};


		[[nodiscard]] RendererWorld() = default;

		[[nodiscard]] explicit RendererWorld(const Core::Vulkan::Context& context)
			: BasicRenderer(context),
			  batch(context, sizeof(Core::Vulkan::Vertex_World), Assets::Sampler::textureNearestSampler),
			  pipelineData{&context},
			  cleanCommand{context.device, commandPool},
			  endBatchCommand{context.device, commandPool},
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

			descriptorBuffer = DescriptorBuffer{this->context().physicalDevice, this->context().device, pipelineData.descriptorSetLayout, pipelineData.descriptorSetLayout.size()};
			descriptorBuffer.load([this](const DescriptorBuffer& descriptorBuffer){
				descriptorBuffer.loadUniform(0, worldUniformBuffer.getBufferAddress(), worldUniformBuffer.requestedSize());
			});

			cameraPropertiesDescriptor.load([this](const DescriptorBuffer& descriptorBuffer){
				descriptorBuffer.loadUniform(0, cameraPropertiesUniformBuffer.getBufferAddress(), cameraPropertiesUniformBuffer.requestedSize());
			});
		}

		void init(const Geom::USize2 size2){
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
			createWrapCommand();
			setPort();
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
			vkQueueWaitIdle(context().device.getPrimaryGraphicsQueue());
			resetCommandPool();

			{
				const auto cmd = batch.obtainTransientCommand();

				baseColor.resize(size2, cmd);
				lightColor.resize(size2, cmd);
				depthStencilAttachment.resize(size2, cmd);
			}
			createDepthView();

			gaussian.resize(size, commandPool_Compute.getTransient(context().device.getPrimaryComputeQueue()), commandPool_Compute.obtain());
			ssao.resize(size, commandPool_Compute.getTransient(context().device.getPrimaryComputeQueue()), commandPool_Compute.obtain());
			merge.resize(size, commandPool_Compute.getTransient(context().device.getPrimaryComputeQueue()), commandPool_Compute.obtain());
			nfaa.resize(size, commandPool_Compute.getTransient(context().device.getPrimaryComputeQueue()), commandPool_Compute.obtain());

			createPipeline();
			createDrawCommands();
			createWrapCommand();
			setPort();
		}

		Core::Vulkan::Attachment& getResult_gaussian(){
			return gaussian.images.back();
		}

		Core::Vulkan::Attachment& getResult_NFAA(){
			return nfaa.images.back();
		}

		Core::Vulkan::Attachment& getResult_SSAO(){
			return ssao.images.back();
		}

		Core::Vulkan::Attachment& getResult_merge(){
			return merge.images.back();
		}

		void doPostProcess() const{
			using namespace Core::Vulkan;

			Util::submitCommand(context().device.getPrimaryGraphicsQueue(), endBatchCommand);
			Util::submitCommand(context().device.getPrimaryComputeQueue(), gaussian.mainCommandBuffer);
			Util::submitCommand(context().device.getPrimaryComputeQueue(), ssao.mainCommandBuffer);
		}

		void doMerge() const{
			using namespace Core::Vulkan;

			Util::submitCommand(context().device.getPrimaryComputeQueue(), merge.mainCommandBuffer);
			Util::submitCommand(context().device.getPrimaryGraphicsQueue(), cleanCommand);

			nfaa.submitCommand();
		}

	private:
		void createPostProcessors(){
			{
				gaussian = Assets::PostProcess::Factory::gaussianFactory.generate({size, [this]{
					AttachmentPort port{};

					port.views.insert_or_assign(0, lightColor.getView());
					port.images.insert_or_assign(0, lightColor.getImage());
					return port;
				}, [this]{
					return std::vector{cameraPropertyDescriptorLayout.get()};
				}, commandPool_Compute.getTransient(context().device.getPrimaryComputeQueue())});

				gaussian.commandRecorderAdditional = [this](VkCommandBuffer scopedCommand){
					cameraPropertiesDescriptor.bindTo(scopedCommand,
						VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR|VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
					);

					Core::Vulkan::EXT::cmdSetDescriptorBufferOffsetsEXT(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE, gaussian.pipelineData.layout,
						1, 1, Core::Vulkan::Seq::Indices<0>, Core::Vulkan::Seq::Offset<0>);
				};

				gaussian.updateDescriptors();
				gaussian.recordCommand(commandPool_Compute.obtain());
			}

			{
				ssao = Assets::PostProcess::Factory::ssaoFactory.generate({size, [this]{
					AttachmentPort port{};

					port.views.insert_or_assign(0, depthAttachmentView);
					port.images.insert_or_assign(0, depthStencilAttachment.getImage());
					return port;
				}, [this]{
					return std::vector{cameraPropertyDescriptorLayout.get()};
				}, commandPool_Compute.getTransient(context().device.getPrimaryComputeQueue())});

				ssao.commandRecorderAdditional = [this](VkCommandBuffer scopedCommand){
					cameraPropertiesDescriptor.bindTo(scopedCommand,
						VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR|VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
					);

					Core::Vulkan::EXT::cmdSetDescriptorBufferOffsetsEXT(
						scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE,
						ssao.pipelineData.layout,
						1, 1, Core::Vulkan::Seq::Indices<0>, Core::Vulkan::Seq::Offset<0>);
				};

				ssao.updateDescriptors();
				ssao.recordCommand(commandPool_Compute.obtain());
			}

			{
				merge = Assets::PostProcess::Factory::worldMergeFactory.generate({size, [this]{
					AttachmentPort port{};

					port.addAttachment(0, baseColor);
					port.addAttachment(1, lightColor);
					port.views.insert_or_assign(2, getResult_gaussian().getView());
					port.views.insert_or_assign(3, getResult_SSAO().getView());

					return port;
				}, {},
					commandPool_Compute.getTransient(context().device.getPrimaryComputeQueue()),
					commandPool_Compute.obtain()});
			}

			{
				nfaa = Assets::PostProcess::Factory::nfaaFactory.generate({size, [this]{
					AttachmentPort port{};

					port.addAttachment(0, getResult_merge());
					return port;
				}, {},
					commandPool_Compute.getTransient(context().device.getPrimaryComputeQueue()),
					commandPool_Compute.obtain()});
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


				ScopedCommand scopedCommand{unit, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};
				Util::bufferBarrier(scopedCommand, barriers);


				{
					dynamicRendering.beginRendering(scopedCommand, {{}, {size.x, size.y}});
					pipelineData.bind(scopedCommand, VK_PIPELINE_BIND_POINT_GRAPHICS);

					const std::array infos{
						descriptorBuffer.getBindInfo(
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
				}

				for (auto && barrier : barriers){
					Util::swapStage(barrier);
				}

				Util::bufferBarrier(scopedCommand, barriers);
			}

		}

		void createWrapCommand(){
			using namespace Core::Vulkan;
			{
				const ScopedCommand scopedCommand{endBatchCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				std::array barriers{
						VkImageMemoryBarrier2{
							.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
							.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
							.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
							.srcQueueFamilyIndex = context().graphicFamily(),
							.dstQueueFamilyIndex = context().computeFamily(),
							.image = baseColor.getImage(),
							.subresourceRange = ImageSubRange::Color
						} | MemoryBarrier2::Image::Default | MemoryBarrier2::Image::Dst_ComputeRead,
						VkImageMemoryBarrier2{
							.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
							.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
							.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
							.srcQueueFamilyIndex = context().graphicFamily(),
							.dstQueueFamilyIndex = context().computeFamily(),
							.image = lightColor.getImage(),
							.subresourceRange = ImageSubRange::Color
						} | MemoryBarrier2::Image::Default | MemoryBarrier2::Image::Dst_ComputeRead,
						VkImageMemoryBarrier2{
							.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
							VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
							.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
							.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
							.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
							.srcQueueFamilyIndex = context().graphicFamily(),
							.dstQueueFamilyIndex = context().computeFamily(),
							.image = depthStencilAttachment.getImage(),
							.subresourceRange = ImageSubRange::DepthStencil
						} | MemoryBarrier2::Image::Default | MemoryBarrier2::Image::Dst_ComputeRead
					};

				Util::imageBarrier(scopedCommand, barriers);
			}


			{
				const ScopedCommand scopedCommand{cleanCommand, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				std::array barriers{
					VkImageMemoryBarrier2{
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.srcQueueFamilyIndex = context().computeFamily(),
						.dstQueueFamilyIndex = context().graphicFamily(),
						.image = baseColor.getImage(),
						.subresourceRange = ImageSubRange::Color
					} | MemoryBarrier2::Image::Default | MemoryBarrier2::Image::Dst_ComputeRead | MemoryBarrier2::Image::Dst_TransferWrite,

					VkImageMemoryBarrier2{
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.image = lightColor.getImage(),
						.subresourceRange = ImageSubRange::Color
					} | MemoryBarrier2::Image::Default | MemoryBarrier2::Image::Dst_ComputeRead | MemoryBarrier2::Image::Dst_TransferWrite,

					VkImageMemoryBarrier2{
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.image = depthStencilAttachment.getImage(),
						.subresourceRange = ImageSubRange::DepthStencil
					} | MemoryBarrier2::Image::Default | MemoryBarrier2::Image::Dst_ComputeRead | MemoryBarrier2::Image::Dst_TransferWrite,
				};

				Util::imageBarrier(cleanCommand, barriers);

				baseColor.getImage().cmdClearColor(scopedCommand, ImageSubRange::Color);
				lightColor.getImage().cmdClearColor(scopedCommand, ImageSubRange::Color);
				depthStencilAttachment.getImage().cmdClearDepthStencil(scopedCommand, ImageSubRange::DepthStencil);

				std::ranges::for_each(barriers, Util::swapStage<VkImageMemoryBarrier2>);

				barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				barriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				barriers[2].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				barriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
				barriers[0].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

				barriers[1].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
				barriers[1].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

				barriers[2].dstStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
				barriers[2].dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

				Util::imageBarrier(cleanCommand, barriers);
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

		void setPort(){
			port.addAttachment(0, getResult_NFAA());
		}
	};
}
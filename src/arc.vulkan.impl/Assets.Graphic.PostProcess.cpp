module;

#include <vulkan/vulkan.h>

module Assets.Graphic.PostProcess;

import Assets.Graphic;

import Core.Vulkan.Concepts;
import Core.Vulkan.Shader;
import Core.Vulkan.Attachment;
import Core.Vulkan.RenderPass;
import Core.Vulkan.Pipeline;
import Core.Vulkan.Util;
import Core.Vulkan.Preinstall;
import Core.Vulkan.Comp;
import Core.Vulkan.Image;
import std;

import Math;

import Core.Vulkan.EXT;

void Assets::PostProcess::load(const Core::Vulkan::Context& context){
	using namespace Core::Vulkan;

	Factory::gaussianFactory = Graphic::ComputePostProcessorFactory{&context};
	Factory::ssaoFactory = Graphic::ComputePostProcessorFactory{&context};
	Factory::nfaaFactory = Graphic::ComputePostProcessorFactory{&context};
	Factory::worldMergeFactory = Graphic::ComputePostProcessorFactory{&context};
	Factory::presentMerge = Graphic::ComputePostProcessorFactory{&context};

	Factory::uiMerge = Graphic::ComputePostProcessorFactory{&context};

	//TODO transfer blur result ownership to compute queue initially
	Factory::gaussianFactory.creator = [](
		const Graphic::ComputePostProcessorFactory& factory,
		Graphic::PostProcessorCreateProperty&& property){
			Graphic::ComputePostProcessor processor{*factory.context, std::move(property.portProv), std::move(property.layoutProv)};
			static constexpr std::uint32_t Passes = 2;

			processor.pipelineData.createDescriptorLayout([](DescriptorSetLayout& layout){
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
			});

			processor.createDescriptorBuffers(4);

			//0 - horizon
			processor.addUniformBuffer(sizeof(UniformBlock_kernalGaussian));
			//1 - vertical
			processor.addUniformBuffer(sizeof(UniformBlock_kernalGaussian));

			processor.pipelineData.createPipelineLayout(0, processor.getAppendedLayout());
			processor.resizeCallback = [](Graphic::ComputePostProcessor& pipeline){
				pipeline.pipelineData.createComputePipeline(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
				                                            Shader::Comp::Gaussian);
			};

			processor.resize(property.size, nullptr, {});

			{
				std::array usages{
						VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
						VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
						VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
					};

				for(auto usage : usages){
					Attachment image{processor.context->physicalDevice, processor.context->device};
					image.create(processor.size(), property.createInitCommandBuffer, usage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_GENERAL);
					processor.images.push_back(std::move(image));
				}
			}

			property.createInitCommandBuffer = {};

			processor.descriptorSetUpdator = [](Graphic::ComputePostProcessor& postProcessor){
				const auto& horiUniformBuffer = postProcessor.uniformBuffers[0];
				const auto& vertUniformBuffer = postProcessor.uniformBuffers[1];

				const VkDescriptorImageInfo imageinfo_input{
						.sampler = Sampler::blitSampler,
						.imageView = postProcessor.port.views.at(0),
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};

				const auto pingpong0 = postProcessor.images[0].getDescriptorInfo(
					Sampler::blitSampler, VK_IMAGE_LAYOUT_GENERAL);
				const auto pingpong1 = postProcessor.images[1].getDescriptorInfo(
					Sampler::blitSampler, VK_IMAGE_LAYOUT_GENERAL);
				const auto imageinfo_output = postProcessor.images[2].getDescriptorInfo(
					nullptr, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

				postProcessor.descriptorBuffers[0].load([&](const DescriptorBuffer& buffer){
					buffer.loadImage(0, imageinfo_input, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
					buffer.loadImage(1, pingpong0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
					buffer.loadUniform(2, horiUniformBuffer.getBufferAddress(), horiUniformBuffer.requestedSize());
				});
				postProcessor.descriptorBuffers[1].load([&](const DescriptorBuffer& buffer){
					buffer.loadImage(0, pingpong0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
					buffer.loadImage(1, pingpong1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
					buffer.loadUniform(2, vertUniformBuffer.getBufferAddress(), vertUniformBuffer.requestedSize());
				});
				postProcessor.descriptorBuffers[2].load([&](const DescriptorBuffer& buffer){
					buffer.loadImage(0, pingpong1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
					buffer.loadImage(1, pingpong0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
					buffer.loadUniform(2, horiUniformBuffer.getBufferAddress(), horiUniformBuffer.requestedSize());
				});
				postProcessor.descriptorBuffers[3].load([&](const DescriptorBuffer& buffer){
					buffer.loadImage(0, pingpong1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
					buffer.loadImage(1, imageinfo_output, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
					buffer.loadUniform(2, vertUniformBuffer.getBufferAddress(), vertUniformBuffer.requestedSize());
				});
			};

			processor.uniformBuffers[0].memory.loadData(GaussianKernalHori);
			processor.uniformBuffers[1].memory.loadData(GaussianKernalVert);

			processor.commandRecorder = [](Graphic::ComputePostProcessor& postProcessor){
				const ScopedCommand scopedCommand{
						postProcessor.mainCommandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
					};

				postProcessor.pipelineData.bind(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE);

				static constexpr Geom::USize2 UnitSize{16, 16};
				auto [ux, uy] = postProcessor.size().add(UnitSize.copy().sub(1, 1)).div(UnitSize);

				std::array barriers = {
					VkImageMemoryBarrier2{
					.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.image = postProcessor.images.back().getImage(),
					.subresourceRange = ImageSubRange::Color
				} | MemoryBarrier2::Image::Default  | MemoryBarrier2::Image::QueueLocal};

				Util::imageBarrier(scopedCommand, barriers);

				auto bindDesc = [&](const std::uint32_t i){
					postProcessor.descriptorBuffers[i].bindTo(scopedCommand,
					                                          VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
					                                          VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
					);
					EXT::cmdSetDescriptorBufferOffsetsEXT(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE,
					                                      postProcessor.pipelineData.layout,
					                                      0, 1, Seq::Indices<0>, Seq::Offset<0>);
					postProcessor.bindAppendedDescriptors(scopedCommand);
				};

				bindDesc(0);
				vkCmdDispatch(scopedCommand, ux, uy, 1);

				for(std::size_t i = 0; i < Passes; ++i){
					bindDesc(1);
					vkCmdDispatch(scopedCommand, ux, uy, 1);

					bindDesc(2);
					vkCmdDispatch(scopedCommand, ux, uy, 1);
				}

				bindDesc(3);
				vkCmdDispatch(scopedCommand, ux, uy, 1);
				Util::swapStage(barriers.back());
				barriers.back().newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				Util::imageBarrier(scopedCommand, barriers);
			};

			return processor;
		};

	Factory::ssaoFactory.creator = [](
		const Graphic::ComputePostProcessorFactory& factory,
		Graphic::PostProcessorCreateProperty&& property){
			Graphic::ComputePostProcessor processor{*factory.context, std::move(property.portProv), std::move(property.layoutProv)};

			processor.pipelineData.createDescriptorLayout([](DescriptorSetLayout& layout){
				layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
			});

			processor.createDescriptorBuffers(1);

			processor.pipelineData.createPipelineLayout(0, processor.getAppendedLayout());
			processor.resizeCallback = [](Graphic::ComputePostProcessor& pipeline){
				pipeline.pipelineData.createComputePipeline(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT, Shader::Comp::SSAO);
			};

			processor.resize(property.size, nullptr, {});
			processor.addUniformBuffer<UniformBlock_kernalSSAO>(UniformBlock_kernalSSAO{processor.size()});

			processor.addImage([&](const Graphic::ComputePostProcessor& p, Attachment& image){
				image.create(
					p.size(), property.createInitCommandBuffer,
					VK_IMAGE_USAGE_STORAGE_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT |
					VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			});

			property.createInitCommandBuffer = {};

			processor.descriptorSetUpdator = [](Graphic::ComputePostProcessor& postProcessor){
				const auto& ubo = postProcessor.uniformBuffers[0];

				const VkDescriptorImageInfo imageInfo_input{
						.sampler = Sampler::blitSampler,
						.imageView = postProcessor.port.views.at(0),
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};

				const auto imageInfo_output =
					postProcessor.images.back().getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

				postProcessor.descriptorBuffers.front().load([&](const DescriptorBuffer& buffer){
					buffer.loadImage(0, imageInfo_input, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
					buffer.loadImage(1, imageInfo_output, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
					buffer.loadUniform(2, ubo.getBufferAddress(), ubo.requestedSize());
				});
			};

			processor.commandRecorder = [](Graphic::ComputePostProcessor& postProcessor){
				const ScopedCommand scopedCommand{
						postProcessor.mainCommandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
					};

				postProcessor.pipelineData.bind(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE);

				VkImageMemoryBarrier2 barrier_resultImage{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.image = postProcessor.images.back().getImage(),
						.subresourceRange = ImageSubRange::Color
					};

				Util::imageBarrier(scopedCommand, std::array{barrier_resultImage});

				postProcessor.descriptorBuffers.front().bindTo(
					scopedCommand,
					VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
					VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
				);
				EXT::cmdSetDescriptorBufferOffsetsEXT(
					scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE,
					postProcessor.pipelineData.layout,
					0, 1, Seq::Indices<0>, Seq::Offset<0>);
				postProcessor.bindAppendedDescriptors(scopedCommand);

				static constexpr Geom::USize2 UnitSize{16, 16};
				const auto [ux, uy] = postProcessor.size().add(UnitSize.copy().sub(1, 1)).div(UnitSize);
				vkCmdDispatch(scopedCommand, ux, uy, 1);

				Util::swapStage(barrier_resultImage);
				Util::imageBarrier(scopedCommand, std::array{barrier_resultImage});
			};

			return processor;
		};

	Factory::worldMergeFactory.creator = [](
		const Graphic::ComputePostProcessorFactory& factory,
		Graphic::PostProcessorCreateProperty&& property){
			Graphic::ComputePostProcessor processor{*factory.context, std::move(property.portProv), std::move(property.layoutProv)};

			static constexpr std::size_t InputAttachmentsCount = 4;

			processor.pipelineData.createDescriptorLayout([](DescriptorSetLayout& layout){
				layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

				//input
				for(std::size_t i = 0; i < InputAttachmentsCount; ++i){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
				}

				//output
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
			});

			processor.createDescriptorBuffers(1);

			processor.pipelineData.createPipelineLayout(0, processor.getAppendedLayout());
			processor.resizeCallback = [](Graphic::ComputePostProcessor& pipeline){
				pipeline.pipelineData.createComputePipeline(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
				                                            Shader::Comp::worldMerge);
			};

			processor.resize(property.size, nullptr, {});

			processor.addImage([&](const Graphic::ComputePostProcessor& p, Attachment& image){
				image.create(
					p.size(), property.createInitCommandBuffer,
					VK_IMAGE_USAGE_STORAGE_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT |
					VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			});

			property.createInitCommandBuffer = {};

			processor.descriptorSetUpdator = [](Graphic::ComputePostProcessor& postProcessor){
				const auto imageInfo_output =
					postProcessor.images.back().getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

				postProcessor.descriptorBuffers.front().load([&](const DescriptorBuffer& buffer){
					for(std::size_t i = 0; i < InputAttachmentsCount; ++i){
						buffer.loadImage(
							i, {
								.sampler = Sampler::blitSampler,
								.imageView = postProcessor.port.views.at(i),
								.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
							}, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
					}
					buffer.loadImage(InputAttachmentsCount, imageInfo_output, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				});
			};


			processor.commandRecorder = [](Graphic::ComputePostProcessor& postProcessor){
				const ScopedCommand scopedCommand{
					postProcessor.mainCommandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
				};

				postProcessor.pipelineData.bind(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE);

				static constexpr Geom::USize2 UnitSize{16, 16};
				const auto [ux, uy] = postProcessor.size().add(UnitSize.copy().sub(1, 1)).div(UnitSize);

				std::array barriers{
						VkImageMemoryBarrier2{
							.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
							.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
							.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
							.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
							.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
							.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							.image = postProcessor.images.back().getImage(),
							.subresourceRange = ImageSubRange::Color
						} | MemoryBarrier2::Image::Default | MemoryBarrier2::Image::QueueLocal
					};

				Util::imageBarrier(scopedCommand, barriers);

				postProcessor.descriptorBuffers.front().bindTo(
					scopedCommand,
					VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
					VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
				);
				EXT::cmdSetDescriptorBufferOffsetsEXT(
					scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE,
					postProcessor.pipelineData.layout,
					0, 1, Seq::Indices<0>, Seq::Offset<0>);

				vkCmdDispatch(scopedCommand, ux, uy, 1);

				Util::swapStage(barriers.back());
				barriers.back().newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				Util::imageBarrier(scopedCommand, barriers);
			};

			processor.updateDescriptors();
			processor.recordCommand(std::move(property.mainCommandBuffer));

			return processor;
		};

	Factory::nfaaFactory.creator = [](
		const Graphic::ComputePostProcessorFactory& factory,
		Graphic::PostProcessorCreateProperty&& property){
			Graphic::ComputePostProcessor processor{*factory.context, std::move(property.portProv), std::move(property.layoutProv)};

			processor.pipelineData.createDescriptorLayout([](DescriptorSetLayout& layout){
				layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
			});

			processor.createDescriptorBuffers(1);

			processor.pipelineData.createPipelineLayout();
			processor.resizeCallback = [](Graphic::ComputePostProcessor& pipeline){
				pipeline.pipelineData.createComputePipeline(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT, Shader::Comp::NFAA);
			};

			processor.resize(property.size, nullptr, {});

			processor.addImage([&](const Graphic::ComputePostProcessor& p, Attachment& image){
				image.create(
					p.size(), property.createInitCommandBuffer,
					VK_IMAGE_USAGE_STORAGE_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT |
					VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			});

			property.createInitCommandBuffer = {};

			processor.descriptorSetUpdator = [](Graphic::ComputePostProcessor& postProcessor){
				const VkDescriptorImageInfo imageInfo_input{
						.sampler = Sampler::blitSampler,
						.imageView = postProcessor.port.views.at(0),
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};

				const auto imageInfo_output =
					postProcessor.images.back().getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

				postProcessor.descriptorBuffers.front().load([&](const DescriptorBuffer& buffer){
					buffer.loadImage(0, imageInfo_input, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
					buffer.loadImage(1, imageInfo_output, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				});
			};

			processor.commandRecorder = [](Graphic::ComputePostProcessor& postProcessor){
				const ScopedCommand scopedCommand{
						postProcessor.mainCommandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
					};

				postProcessor.pipelineData.bind(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE);

				static constexpr Geom::USize2 UnitSize{16, 16};
				const auto [ux, uy] = postProcessor.size().add(UnitSize.copy().sub(1, 1)).div(UnitSize);

				std::array barrier_resultImage{VkImageMemoryBarrier2{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.image = postProcessor.images.back().getImage(),
						.subresourceRange = ImageSubRange::Color
					}};

				Util::imageBarrier(scopedCommand, barrier_resultImage);

				postProcessor.descriptorBuffers.front().bindTo(
					scopedCommand,
					VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
					VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
				);
				EXT::cmdSetDescriptorBufferOffsetsEXT(
					scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE,
					postProcessor.pipelineData.layout,
					0, 1, Seq::Indices<0>, Seq::Offset<0>);

				vkCmdDispatch(scopedCommand, ux, uy, 1);

				Util::swapStage(barrier_resultImage[0]);
				barrier_resultImage[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				Util::imageBarrier(scopedCommand, barrier_resultImage);
			};

			processor.updateDescriptors();
			processor.recordCommand(std::move(property.mainCommandBuffer));

			return processor;
		};

	Factory::presentMerge.creator = [](
		const Graphic::ComputePostProcessorFactory& factory,
		Graphic::PostProcessorCreateProperty&& property){
			Graphic::ComputePostProcessor processor{*factory.context, std::move(property.portProv), std::move(property.layoutProv)};

			processor.pipelineData.createDescriptorLayout([](DescriptorSetLayout& layout){
				layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
			});

			processor.createDescriptorBuffers(1);

			processor.pipelineData.createPipelineLayout();
			processor.resizeCallback = [](Graphic::ComputePostProcessor& pipeline){
				pipeline.pipelineData.createComputePipeline(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT, Shader::Comp::presentMerge);
			};

			processor.resize(property.size, nullptr, {});

			processor.addImage([&](const Graphic::ComputePostProcessor& p, Attachment& image){
				image.create(
					p.size(), property.createInitCommandBuffer,
					VK_IMAGE_USAGE_STORAGE_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT |
					VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
					VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			});

			property.createInitCommandBuffer = {};

			processor.descriptorSetUpdator = [](Graphic::ComputePostProcessor& postProcessor){
				const auto imageInfo_output =
					postProcessor.images.back().getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

				postProcessor.descriptorBuffers.front().load([&](const DescriptorBuffer& buffer){
					for(std::size_t i = 0; i < 3; ++i){
						const VkDescriptorImageInfo imageInfo_input{
								.sampler = Sampler::blitSampler,
								.imageView = postProcessor.port.views.at(i),
								.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
							};
						buffer.loadImage(i, imageInfo_input, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
					}
					buffer.loadImage(3, imageInfo_output, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				});
			};

			processor.commandRecorder = [](Graphic::ComputePostProcessor& postProcessor){
				const ScopedCommand scopedCommand{
						postProcessor.mainCommandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
					};

				std::array barriers = {
					VkImageMemoryBarrier2{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
						.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.srcQueueFamilyIndex = postProcessor.context->graphicFamily(),
						.dstQueueFamilyIndex = postProcessor.context->computeFamily(),
						.image = postProcessor.images.back().getImage(),
						.subresourceRange = ImageSubRange::Color
					},
				};

				Util::imageBarrier(scopedCommand, barriers);

				postProcessor.pipelineData.bind(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE);

				postProcessor.descriptorBuffers.front().bindTo(
					scopedCommand,
					VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
					VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
				);

				EXT::cmdSetDescriptorBufferOffsetsEXT(
					scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE,
					postProcessor.pipelineData.layout,
					0, 1, Seq::Indices<0>, Seq::Offset<0>);

				static constexpr Geom::USize2 UnitSize{16, 16};
				const auto [ux, uy] = postProcessor.size().add(UnitSize.copy().sub(1, 1)).div(UnitSize);

				vkCmdDispatch(scopedCommand, ux, uy, 1);

				Util::swapStage(barriers);
				Util::imageBarrier(scopedCommand, barriers);
			};

			processor.updateDescriptors();
			processor.recordCommand(std::move(property.mainCommandBuffer));

			return processor;
		};


	Factory::uiMerge.creator = [](
		const Graphic::ComputePostProcessorFactory& factory,
		Graphic::PostProcessorCreateProperty&& property){
			Graphic::ComputePostProcessor processor{*factory.context, std::move(property.portProv), std::move(property.layoutProv)};

			processor.pipelineData.createDescriptorLayout([](DescriptorSetLayout& layout){
				layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
			});

			processor.createDescriptorBuffers(1);

			processor.pipelineData.createPipelineLayout();
			processor.resizeCallback = [](Graphic::ComputePostProcessor& pipeline){
				pipeline.pipelineData.createComputePipeline(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT, Shader::Comp::uiMerge);
			};

			processor.resize(property.size, nullptr, {});

			processor.addImage([&](const Graphic::ComputePostProcessor& p, Attachment& image){
				image.create(
					p.size(), property.createInitCommandBuffer,
					VK_IMAGE_USAGE_STORAGE_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT |
					VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			});

			processor.addImage([&](const Graphic::ComputePostProcessor& p, Attachment& image){
				image.create(
					p.size(), property.createInitCommandBuffer,
					VK_IMAGE_USAGE_STORAGE_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT |
					VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			});

			property.createInitCommandBuffer = {};

			processor.descriptorSetUpdator = [](Graphic::ComputePostProcessor& postProcessor){
				postProcessor.descriptorBuffers.front().load([&](const DescriptorBuffer& buffer){
					for(std::size_t i = 0; i < 3; ++i){
						const VkDescriptorImageInfo imageInfo_input{
								.sampler = Sampler::blitSampler,
								.imageView = postProcessor.port.views.at(i),
								.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
							};
						buffer.loadImage(i, imageInfo_input, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
					}

					for(std::size_t i = 0; i < 2; ++i){
						const VkDescriptorImageInfo imageInfo_output{
								.sampler = Sampler::blitSampler,
								.imageView = postProcessor.images[i].getView(),
								.imageLayout = VK_IMAGE_LAYOUT_GENERAL
							};

						buffer.loadImage(i + 3, imageInfo_output, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
					}
				});
			};

			processor.commandRecorder = [](Graphic::ComputePostProcessor& postProcessor){
				const ScopedCommand scopedCommand{
						postProcessor.mainCommandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
					};

				std::array<VkImageMemoryBarrier2, 5> barriers{};

				for(std::size_t i = 0; i < 3; ++i){
					barriers[i] = VkImageMemoryBarrier2{
							.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
							.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
							.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
							.srcQueueFamilyIndex = postProcessor.context->graphicFamily(),
							.dstQueueFamilyIndex = postProcessor.context->computeFamily(),
							.image = postProcessor.port.images.at(i),
							.subresourceRange = ImageSubRange::Color
						} | MemoryBarrier2::Image::Default | MemoryBarrier2::Image::Dst_ComputeWrite;
				}

				for(std::size_t i = 0; i < 2; ++i){
					barriers[i + 3] =
						VkImageMemoryBarrier2{
							.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
							.newLayout = VK_IMAGE_LAYOUT_GENERAL,
							.image = postProcessor.images[i].getImage(),
							.subresourceRange = ImageSubRange::Color
						}
					| MemoryBarrier2::Image::Default
					| MemoryBarrier2::Image::QueueLocal
					| MemoryBarrier2::Image::Src_ComputeRead
					| MemoryBarrier2::Image::Dst_ComputeWrite;
				}


				Util::imageBarrier(scopedCommand, barriers);

				postProcessor.pipelineData.bind(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE);

				postProcessor.descriptorBuffers.front().bindTo(
					scopedCommand,
					VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
					VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
				);

				EXT::cmdSetDescriptorBufferOffsetsEXT(
					scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE,
					postProcessor.pipelineData.layout,
					0, 1, Seq::Indices<0>, Seq::Offset<0>);

				static constexpr Geom::USize2 UnitSize{16, 16};
				const auto [ux, uy] = postProcessor.size().add(UnitSize.copy().sub(1, 1)).div(UnitSize);

				vkCmdDispatch(scopedCommand, ux, uy, 1);

				Util::swapStage(barriers);
				Util::imageBarrier(scopedCommand, barriers);
			};

			processor.updateDescriptors();
			processor.recordCommand(std::move(property.mainCommandBuffer));

			return processor;
		};
}

void Assets::PostProcess::dispose(){}

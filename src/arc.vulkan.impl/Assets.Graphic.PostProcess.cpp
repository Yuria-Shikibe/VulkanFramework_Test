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

	Factory::blurProcessorFactory = Graphic::GraphicPostProcessorFactory{&context};
	Factory::mergeBloomFactory = Graphic::GraphicPostProcessorFactory{&context};
	Factory::game_uiMerge = Graphic::GraphicPostProcessorFactory{&context};
	Factory::nfaaFactory = Graphic::GraphicPostProcessorFactory{&context};
	Factory::gaussianFactory = Graphic::ComputePostProcessorFactory{&context};
	Factory::ssaoFactory = Graphic::ComputePostProcessorFactory{&context};

	Factory::blurProcessorFactory.creator = [](
		const Graphic::GraphicPostProcessorFactory& factory, Graphic::PortProv&& portProv, const Geom::USize2 size){
			Graphic::GraphicPostProcessor processor{*factory.context, std::move(portProv)};
			static constexpr std::uint32_t Passes = 4;
			// static constexpr float Scale = 0.5f;

			processor.renderProcedure.createRenderPass([](RenderPass& renderPass){
				{
					renderPass.pushAttachment( //port.in[0]
						VkAttachmentDescription{}
						| AttachmentDesc::Default
						| AttachmentDesc::Stencil_DontCare
						| AttachmentDesc::ImportAttachment<>
						| AttachmentDesc::Store_Store);

					renderPass.pushAttachment( //Pingpong 1
						VkAttachmentDescription{}
						| AttachmentDesc::Default
						| AttachmentDesc::Stencil_DontCare
						| AttachmentDesc::ReadAndWrite
						| AttachmentDesc::ReusedColorAttachment);

					renderPass.pushAttachment( //Pingpong 2
						VkAttachmentDescription{}
						| AttachmentDesc::Default
						| AttachmentDesc::Stencil_DontCare
						| AttachmentDesc::ReadAndWrite
						| AttachmentDesc::ReusedColorAttachment);

					renderPass.pushAttachment( //port.out[0]
						VkAttachmentDescription{}
						| AttachmentDesc::Default
						| AttachmentDesc::Stencil_DontCare
						| AttachmentDesc::ExportAttachment
						| AttachmentDesc::Load_DontCare);
				}

				renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
					subpass.addOutput(1);
				});

				//Pingpong
				for(std::uint32_t i = 0; i < Passes - 1; ++i){
					for(bool offset : {false, true}){
						renderPass.pushSubpass([offset](RenderPass::SubpassData& subpass){
							subpass.addOutput(2 - offset);
							subpass.addReserved(2 - !offset);

							subpass.addDependency(VkSubpassDependency{
									.srcSubpass = subpass.getPrevIndex(),
								} | Subpass::Dependency::Blit);
						});
					}
				}

				renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
					subpass.addOutput(3);

					subpass.addDependency(VkSubpassDependency{
							.srcSubpass = subpass.getPrevIndex(),
						} | Subpass::Dependency::Blit);
				});
			});

			processor.renderProcedure.addPipeline([&](PipelineData& data){
				data.addTarget(std::views::iota(0u, Passes * 2));

				data.createDescriptorLayout([](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
				});

				data.pushConstant({VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BlurConstants)});

				data.createDescriptorSet(3);

				data.createPipelineLayout();

				data.builder = [](PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.setColorBlend(&Default::ColorBlending<std::array{Blending::Disable}>)
						.setShaderChain({&Shader::Vert::blitWithUV, &Shader::Frag::blitBlur})
						.setStaticViewportAndScissor(pipeline.size.x, pipeline.size.y);

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			processor.renderProcedure.resize(size);

			processor.createFramebuffer([&](FramebufferLocal& framebuffer){
				constexpr auto PingPongUsage =
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
				auto command = processor.obtainTransientCommand();

				ColorAttachment pingpong1{processor.context->physicalDevice, processor.context->device};
				// pingpong1.setScale(Scale);
				pingpong1.create(size, command, PingPongUsage);

				ColorAttachment pingpong2{processor.context->physicalDevice, processor.context->device};
				// pingpong2.setScale(Scale);
				pingpong2.create(size, command, PingPongUsage);

				//submit command
				command = {};

				framebuffer.addCapturedAttachments(1, std::move(pingpong1));
				framebuffer.addCapturedAttachments(2, std::move(pingpong2));
			});

			processor.commandRecorder = [](Graphic::GraphicPostProcessor& postProcessor){
				ScopedCommand scopedCommand{postProcessor.commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				const auto info = postProcessor.renderProcedure.getBeginInfo(postProcessor.framebuffer);
				vkCmdBeginRenderPass(scopedCommand, &info, VK_SUBPASS_CONTENTS_INLINE);

				auto cmdContext = postProcessor.renderProcedure.startCmdContext(scopedCommand);

				const auto unitStep = ~postProcessor.size().as<float>() * 1.25f;

				const std::array arr{
						BlurConstants{
							//sample X
							unitStep * Geom::norXVec2<float> * BlurConstants::UnitNear,
							unitStep * Geom::norXVec2<float> * BlurConstants::UnitFar,
						},
						BlurConstants{
							//sample Y
							unitStep * Geom::norYVec2<float> * BlurConstants::UnitNear,
							unitStep * Geom::norYVec2<float> * BlurConstants::UnitFar,
						}
					};

				cmdContext.data().bindDescriptorTo(scopedCommand, 0);
				cmdContext.data().bindConstantToSeq(scopedCommand, arr[0]);
				scopedCommand->blitDraw();
				cmdContext.next();

				for(bool flag{false}; const auto& step : arr){
					for(std::uint32_t i = 0; i < Passes - 1; ++i){
						cmdContext.data().bindDescriptorTo(scopedCommand, 1 + flag);
						cmdContext.data().bindConstantToSeq(scopedCommand, step);
						scopedCommand->blitDraw();
						cmdContext.next();
						flag = !flag;
					}
				}

				cmdContext.data().bindDescriptorTo(scopedCommand, 1);
				cmdContext.data().bindConstantToSeq(scopedCommand, arr[1]);
				scopedCommand->blitDraw();
				cmdContext.next();

				vkCmdEndRenderPass(scopedCommand);
			};

			return processor;
		};

	Factory::mergeBloomFactory.creator = [](
		const Graphic::GraphicPostProcessorFactory& factory, Graphic::PortProv&& portProv, const Geom::USize2 size){
			Graphic::GraphicPostProcessor processor{*factory.context, std::move(portProv)};

			processor.renderProcedure.createRenderPass([](RenderPass& renderPass){
				{
					renderPass.pushAttachment( //port.in[0] tex source
						VkAttachmentDescription{}
						| AttachmentDesc::Default
						| AttachmentDesc::Stencil_DontCare
						| AttachmentDesc::ImportAttachment<>);

					renderPass.pushAttachment( //port.in[1] light source
						VkAttachmentDescription{}
						| AttachmentDesc::Default
						| AttachmentDesc::Stencil_DontCare
						| AttachmentDesc::ImportAttachment<>);

					renderPass.pushAttachment( //port.in[2] blurred
						VkAttachmentDescription{}
						| AttachmentDesc::Default
						| AttachmentDesc::Stencil_DontCare
						| AttachmentDesc::ImportAttachment<VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>);

					renderPass.pushAttachment( //local [3] SSAO result
						VkAttachmentDescription{
							.format = VK_FORMAT_R8G8B8A8_UNORM,
							.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						}
						| AttachmentDesc::Default
						| AttachmentDesc::Stencil_DontCare
						| AttachmentDesc::ReadAndWrite
					);

					//TODO shade
					renderPass.pushAttachment( //port.out[0]
						VkAttachmentDescription{}
						| AttachmentDesc::Default
						| AttachmentDesc::Stencil_DontCare
						| AttachmentDesc::ExportAttachment
						| AttachmentDesc::Load_DontCare);
				}

				//SSAO
				renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
					//TODO
					// subpass.addDependency(VkSubpassDependency{
					//                 .srcSubpass = subpass.index,
					//                 .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					//                 .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					//                 .srcAccessMask = 0,
					//                 .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
					//                 .dependencyFlags = VK_DEPENDENCY_VIEW_LOCAL_BIT | VK_DEPENDENCY_BY_REGION_BIT
					//             });

					subpass.addOutput(3);
				});

				//Merge
				renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
					subpass.addInput(0);
					subpass.addInput(1);
					subpass.addInput(2);
					subpass.addInput(3);
					subpass.addOutput(4);

					subpass.addDependency(VkSubpassDependency{
							.srcSubpass = subpass.getPrevIndex()
						} | Subpass::Dependency::Blit);
				});
			});

			//SSAO
			processor.renderProcedure.pushAndInitPipeline([&](PipelineData& data){
				data.createDescriptorLayout([](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
				});

				// data.pushConstant({VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BlurConstants)});

				data.createDescriptorSet(1);
				data.addUniformBuffer(sizeof(UniformBlock_kernalSSAO));
				data.createPipelineLayout();

				data.builder = [](PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.setColorBlend(&Default::ColorBlending<std::array{Blending::Disable}>)
						.setShaderChain({&Shader::Vert::blitWithUV, &Shader::Frag::SSAO})
						.setStaticViewportAndScissor(pipeline.size.x, pipeline.size.y);

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			processor.renderProcedure.pushAndInitPipeline([&](PipelineData& data){
				data.createDescriptorLayout([](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
				});

				// data.pushConstant({VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BlurConstants)});

				data.createDescriptorSet(1);

				data.createPipelineLayout();

				data.builder = [](PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.setColorBlend(&Default::ColorBlending<std::array{Blending::Disable}>)
						.setShaderChain({&Shader::Vert::blitSingle, &Shader::Frag::blitMerge})
						.setStaticViewportAndScissor(pipeline.size.x, pipeline.size.y);

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			processor.renderProcedure.resize(size);

			processor.createFramebuffer([&](FramebufferLocal& framebuffer){
				ColorAttachment SSAO_Result{processor.context->physicalDevice, processor.context->device};
				auto command = processor.obtainTransientCommand();

				SSAO_Result.create(size, command, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
				command = {};

				framebuffer.addCapturedAttachments(3, std::move(SSAO_Result));
			});

			return processor;
		};

	Factory::nfaaFactory.creator = [](
		const Graphic::GraphicPostProcessorFactory& factory, Graphic::PortProv&& portProv, const Geom::USize2 size){
			Graphic::GraphicPostProcessor processor{*factory.context, std::move(portProv)};

			processor.renderProcedure.createRenderPass([](RenderPass& renderPass){
				renderPass.pushAttachment( //port.out[0]
					VkAttachmentDescription{}
					| AttachmentDesc::Default
					| AttachmentDesc::Stencil_DontCare
					| AttachmentDesc::ExportAttachment
					| AttachmentDesc::Load_DontCare);

				renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
					subpass.addOutput(0);
				});
			});

			processor.renderProcedure.pushAndInitPipeline([&](PipelineData& data){
				data.createDescriptorLayout([](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
				});

				data.pushConstant({VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Geom::Vec2)});

				data.createDescriptorSet(1);

				data.createPipelineLayout();

				data.builder = [](PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.setColorBlend(&Default::ColorBlending<std::array{Blending::Disable}>)
						.setShaderChain({&Shader::Vert::blitWithUV, &Shader::Frag::NFAA})
						.setStaticViewportAndScissor(pipeline.size.x, pipeline.size.y);

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			processor.renderProcedure.resize(size);

			processor.createFramebuffer([&](FramebufferLocal& framebuffer){
				ColorAttachment nfaaResultAttachment{processor.context->physicalDevice, processor.context->device};
				nfaaResultAttachment.create(
					size, processor.obtainTransientCommand(),
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
				);

				framebuffer.pushCapturedAttachments(std::move(nfaaResultAttachment));
			});

			processor.commandRecorder = [](Graphic::GraphicPostProcessor& postProcessor){
				ScopedCommand scopedCommand{postProcessor.commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				const auto info = postProcessor.renderProcedure.getBeginInfo(postProcessor.framebuffer);
				vkCmdBeginRenderPass(scopedCommand, &info, VK_SUBPASS_CONTENTS_INLINE);

				auto cmdContext = postProcessor.renderProcedure.startCmdContext(scopedCommand);

				auto step = ~postProcessor.size().as<float>();
				cmdContext.data().bindDescriptorTo(scopedCommand);
				cmdContext.data().bindConstantToSeq(scopedCommand, step);

				scopedCommand->blitDraw();

				vkCmdEndRenderPass(scopedCommand);
			};

			return processor;
		};

	Factory::game_uiMerge.creator = [](
		const Graphic::GraphicPostProcessorFactory& factory, Graphic::PortProv&& portProv, const Geom::USize2 size){
			Graphic::GraphicPostProcessor processor{*factory.context, std::move(portProv)};

			processor.renderProcedure.createRenderPass([](RenderPass& renderPass){
				auto inputDesc = VkAttachmentDescription{
						.format = VK_FORMAT_R8G8B8A8_UNORM,
						.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
						.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
					}
					| AttachmentDesc::Default
					| AttachmentDesc::Stencil_DontCare;


				//world 0
				renderPass.pushAttachment(inputDesc);

				//ui 0
				renderPass.pushAttachment(inputDesc);

				//ui 1
				renderPass.pushAttachment(inputDesc);

				renderPass.pushAttachment(
					VkAttachmentDescription{}
					| AttachmentDesc::Default
					| AttachmentDesc::Stencil_DontCare
					| AttachmentDesc::ExportAttachment
					| AttachmentDesc::Load_DontCare);

				renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
					subpass.addInput(0);
					subpass.addInput(1);
					subpass.addInput(2);
					subpass.addOutput(3);
				});
			});

			processor.renderProcedure.pushAndInitPipeline([&](PipelineData& data){
				data.createDescriptorLayout([](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
				});

				data.createDescriptorSet(1);

				data.createPipelineLayout();

				data.builder = [](PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.setColorBlend(&Default::ColorBlending<std::array{Blending::Disable}>)
						.setShaderChain({&Shader::Vert::blitSingle, &Shader::Frag::game_ui_merge})
						.setStaticViewportAndScissor(pipeline.size.x, pipeline.size.y);

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			processor.renderProcedure.resize(size);

			processor.createFramebuffer([&](FramebufferLocal& framebuffer){
				ColorAttachment gameUImergeResultAttachment{processor.context->physicalDevice, processor.context->device};
				gameUImergeResultAttachment.create(
					size, processor.obtainTransientCommand(),
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
				);
				framebuffer.addCapturedAttachments(3, std::move(gameUImergeResultAttachment));
			});

			processor.descriptorSetUpdator = [](Graphic::GraphicPostProcessor& postProcessor){
				DescriptorSetUpdator updator{
						postProcessor.context->device, postProcessor.renderProcedure.front().descriptorSets.front()
					};

				std::array<VkDescriptorImageInfo, 3> infos{};

				for(const auto& [index, info] : infos | std::views::enumerate){
					info = postProcessor.framebuffer.getInputInfo(index, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
					updator.pushAttachment(info);
				}

				updator.update();
			};

			processor.commandRecorder = [](Graphic::GraphicPostProcessor& postProcessor){
				ScopedCommand scopedCommand{postProcessor.commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				const auto info = postProcessor.renderProcedure.getBeginInfo(postProcessor.framebuffer);
				vkCmdBeginRenderPass(scopedCommand, &info, VK_SUBPASS_CONTENTS_INLINE);

				auto cmdContext = postProcessor.renderProcedure.startCmdContext(scopedCommand);

				cmdContext.data().bindDescriptorTo(scopedCommand);

				scopedCommand->blitDraw();

				vkCmdEndRenderPass(scopedCommand);
			};

			processor.updateDescriptors();
			processor.recordCommand();

			return processor;
		};

	Factory::gaussianFactory.creator = [](
		const Graphic::ComputePostProcessorFactory& factory,
		Graphic::PortProv&& portProv,
		Graphic::DescriptorLayoutProv&& layoutProv,
		const Geom::USize2 size){
			Graphic::ComputePostProcessor processor{*factory.context, std::move(portProv), std::move(layoutProv)};
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
				pipeline.pipelineData.createComputePipeline(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT, Shader::Comp::Gaussian);
			};

			processor.resize(size);

			{
				std::array usages{
						VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
						VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
						VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
					};

				const auto command = processor.obtainTransientCommand(true);

				for(auto usage : usages){
					Attachment image{processor.context->physicalDevice, processor.context->device};
					image.create(processor.size(), command, usage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_GENERAL);
					processor.images.push_back(std::move(image));
				}
			}

			processor.descriptorSetUpdator = [](Graphic::ComputePostProcessor& postProcessor){
					const auto& horiUniformBuffer = postProcessor.uniformBuffers[0];
					const auto& vertUniformBuffer = postProcessor.uniformBuffers[1];

					const VkDescriptorImageInfo imageinfo_input{
							.sampler = Sampler::blitSampler,
							.imageView = postProcessor.port.in.at(0),
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
						postProcessor.commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
					};

				postProcessor.pipelineData.bind(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE);

				static constexpr Geom::USize2 UnitSize{16, 16};
				auto [ux, uy] = postProcessor.size().add(UnitSize.copy().sub(1, 1)).div(UnitSize);

				{
					Util::transitionImageQueueOwnership(
					   scopedCommand, postProcessor.port.toTransferOwnership.at(0), {
						   .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						   .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						   .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
						   .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
						   .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						   .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
					   },
					   postProcessor.context->physicalDevice.queues.graphicsFamily,
					   postProcessor.context->physicalDevice.queues.computeFamily, ImageSubRange::Color);
				}

				auto bindDesc = [&](const std::uint32_t i){
					postProcessor.descriptorBuffers[i].bindTo(scopedCommand,
						VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR|VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
					);
					EXT::cmdSetDescriptorBufferOffsetsEXT(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE, postProcessor.pipelineData.layout,
						0, 1, Seq::Indices<0>, Seq::Offset<0>);
					postProcessor.bindAppendedDescriptors(scopedCommand);
				};

				bindDesc(0);
				vkCmdDispatch(scopedCommand, ux, uy, 1);

				{
					Util::transitionImageQueueOwnership(
					   scopedCommand, postProcessor.port.toTransferOwnership.at(0), {
						   .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						   .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						   .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
						   .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
						   .srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						   .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
					   },
					   postProcessor.context->physicalDevice.queues.computeFamily,
					   postProcessor.context->physicalDevice.queues.graphicsFamily, ImageSubRange::Color);
				}


				Util::transitionImageQueueOwnership(
					scopedCommand, postProcessor.images.back().getImage(), {
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
						.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
						.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
					},
					postProcessor.context->physicalDevice.queues.graphicsFamily,
					postProcessor.context->physicalDevice.queues.computeFamily, ImageSubRange::Color);

				for(std::size_t i = 0; i < Passes; ++i){
					bindDesc(1);
					vkCmdDispatch(scopedCommand, ux, uy, 1);

					bindDesc(2);
					vkCmdDispatch(scopedCommand, ux, uy, 1);
				}

				bindDesc(3);
				vkCmdDispatch(scopedCommand, ux, uy, 1);

				Util::transitionImageQueueOwnership(
					scopedCommand, postProcessor.images.back().getImage(), {
						.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
						.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
						.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
					},
					postProcessor.context->physicalDevice.queues.computeFamily,
					postProcessor.context->physicalDevice.queues.graphicsFamily, ImageSubRange::Color);
			};

			return processor;
		};

	Factory::ssaoFactory.creator = [](
		const Graphic::ComputePostProcessorFactory& factory,
		Graphic::PortProv&& portProv,
		Graphic::DescriptorLayoutProv&& layoutProv,
		const Geom::USize2 size){
			Graphic::ComputePostProcessor processor{*factory.context, std::move(portProv), std::move(layoutProv)};

			processor.pipelineData.createDescriptorLayout([](DescriptorSetLayout& layout){
				layout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
				layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
			});

			processor.createDescriptorBuffers(1);
			processor.addUniformBuffer<UniformBlock_kernalSSAO>(UniformBlock_kernalSSAO{processor.size()});

			processor.pipelineData.createPipelineLayout(0, processor.getAppendedLayout());
			processor.resizeCallback = [](Graphic::ComputePostProcessor& pipeline){
				pipeline.pipelineData.createComputePipeline(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT, Shader::Comp::Gaussian);
			};

			processor.resize(size);

			processor.addImage([](const Graphic::ComputePostProcessor& p, Attachment& image){
				image.create(
					p.size(), p.obtainTransientCommand(true),
					VK_IMAGE_USAGE_STORAGE_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT |
					VK_IMAGE_USAGE_SAMPLED_BIT |
					VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
					VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_GENERAL);
			});

			processor.descriptorSetUpdator = [](Graphic::ComputePostProcessor& postProcessor){
					const auto& ubo = postProcessor.uniformBuffers[0];

					const VkDescriptorImageInfo imageInfo_input{
							.sampler = Sampler::blitSampler,
							.imageView = postProcessor.port.in.at(0),
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
						postProcessor.commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
					};

				postProcessor.pipelineData.bind(scopedCommand, VK_PIPELINE_BIND_POINT_COMPUTE);

				static constexpr Geom::USize2 UnitSize{16, 16};
				const auto [ux, uy] = postProcessor.size().add(UnitSize.copy().sub(1, 1)).div(UnitSize);

				VkImageMemoryBarrier2 barrier_depthImage{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
					.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = postProcessor.context->graphicFamily(),
					.dstQueueFamilyIndex = postProcessor.context->computeFamily(),
					.image = postProcessor.port.toTransferOwnership.at(0),
					.subresourceRange = ImageSubRange::DepthStencil
				};

				VkImageMemoryBarrier2 barrier_resultImage{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.srcQueueFamilyIndex = postProcessor.context->graphicFamily(),
					.dstQueueFamilyIndex = postProcessor.context->computeFamily(),
					.image = postProcessor.images.back().getImage(),
					.subresourceRange = ImageSubRange::Color
				};

				Util::imageBarrier<2>(scopedCommand, {barrier_depthImage, barrier_resultImage});

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

				vkCmdDispatch(scopedCommand, ux, uy, 1);

				Util::swapStage(barrier_depthImage);
				Util::swapStage(barrier_resultImage);
				Util::imageBarrier<2>(scopedCommand, {barrier_depthImage, barrier_resultImage});
			};

			return processor;
		};
}

void Assets::PostProcess::dispose(){}

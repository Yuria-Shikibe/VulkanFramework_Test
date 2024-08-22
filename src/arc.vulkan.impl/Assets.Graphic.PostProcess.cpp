module;

#include <vulkan/vulkan.h>

module Assets.Graphic.PostProcess;

import Assets.Graphic;

import Core.Vulkan.Concepts;
import Core.Vulkan.Shader;
import Core.Vulkan.Attachment;
import Core.Vulkan.RenderPass;
import Core.Vulkan.Pipeline;
import Core.Vulkan.Preinstall;
import Core.Vulkan.Comp;
import Core.Vulkan.Image;
import std;

void Assets::PostProcess::load(const Core::Vulkan::Context& context){
	using namespace Core::Vulkan;

	Factory::blurProcessorFactory = Graphic::PostProcessorFactory{&context};
	Factory::mergeBloomFactory = Graphic::PostProcessorFactory{&context};
	Factory::nfaaFactory = Graphic::PostProcessorFactory{&context};

	Factory::blurProcessorFactory.creator = [](
		const Graphic::PostProcessorFactory& factory, Graphic::PostProcessor::PortProv&& portProv, const Geom::USize2 size){
			Graphic::PostProcessor processor{*factory.context, std::move(portProv)};
			static constexpr std::uint32_t Passes = 5;
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

			processor.renderProcedure.addPipeline([&](RenderProcedure::PipelineData& data){
				data.addTarget(std::views::iota(0u, Passes * 2));

				data.createDescriptorLayout([](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
				});

				data.pushConstant({VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BlurConstants)});

				data.createDescriptorSet(3);

				data.createPipelineLayout();

				data.builder = [](RenderProcedure::PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.setColorBlend(&Default::ColorBlending<std::array{Blending::Disable}>)
						.setShaderChain({&Shader::Vert::blitWithUV, &Shader::Frag::blitBlur})
						.setStaticViewportAndScissor(pipeline.size().x, pipeline.size().y);

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

			processor.descriptorSetUpdator = [](Graphic::PostProcessor& postProcessor){
				auto& set = postProcessor.renderProcedure.front().descriptorSets;

				constexpr std::array layouts{
					VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
				};

				for(std::size_t i = 0; i < set.size(); ++i){
					DescriptorSetUpdator updator{postProcessor.context->device, set[i]};
					auto imageInfo = Sampler::blitSampler.getDescriptorInfo_ShaderRead(
						postProcessor.framebuffer.at(i),
						layouts[i]
						);
					updator.pushSampledImage(imageInfo);
					updator.update();
				}
			};

			processor.commandRecorder = [](Graphic::PostProcessor& postProcessor){
				ScopedCommand scopedCommand{postProcessor.commandBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};

				const auto info = postProcessor.renderProcedure.getBeginInfo(postProcessor.framebuffer);
				vkCmdBeginRenderPass(scopedCommand, &info, VK_SUBPASS_CONTENTS_INLINE);

				auto cmdContext = postProcessor.renderProcedure.startCmdContext(scopedCommand);

				const auto unitStep = ~postProcessor.size().as<float>() * 1.15f;

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

			processor.updateDescriptors();
			processor.recordCommand();

			return processor;
		};

	Factory::mergeBloomFactory.creator = [](
		const Graphic::PostProcessorFactory& factory, Graphic::PostProcessor::PortProv&& portProv, const Geom::USize2 size){
			Graphic::PostProcessor processor{*factory.context, std::move(portProv)};

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
			processor.renderProcedure.pushAndInitPipeline([&](RenderProcedure::PipelineData& data){
				data.createDescriptorLayout([](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
				});

				// data.pushConstant({VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BlurConstants)});

				data.createDescriptorSet(1);
			    data.addUniformBuffer(sizeof(UniformBlock_SSAO));
				data.createPipelineLayout();

				data.builder = [](RenderProcedure::PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.setColorBlend(&Default::ColorBlending<std::array{Blending::Disable}>)
						.setShaderChain({&Shader::Vert::blitWithUV, &Shader::Frag::SSAO})
						.setStaticViewportAndScissor(pipeline.size().x, pipeline.size().y);

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			processor.renderProcedure.pushAndInitPipeline([&](RenderProcedure::PipelineData& data){
				data.createDescriptorLayout([](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
				});

				// data.pushConstant({VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BlurConstants)});

				data.createDescriptorSet(1);

				data.createPipelineLayout();

				data.builder = [](RenderProcedure::PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.setColorBlend(&Default::ColorBlending<std::array{Blending::Disable}>)
						.setShaderChain({&Shader::Vert::blitSingle, &Shader::Frag::blitMerge})
						.setStaticViewportAndScissor(pipeline.size().x, pipeline.size().y);

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
		const Graphic::PostProcessorFactory& factory, Graphic::PostProcessor::PortProv&& portProv, const Geom::USize2 size){
			Graphic::PostProcessor processor{*factory.context, std::move(portProv)};

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

			processor.renderProcedure.pushAndInitPipeline([&](RenderProcedure::PipelineData& data){
				data.createDescriptorLayout([](DescriptorSetLayout& layout){
					layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
				});

				data.pushConstant({VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Geom::Vec2)});

				data.createDescriptorSet(1);

				data.createPipelineLayout();

				data.builder = [](RenderProcedure::PipelineData& pipeline){
					PipelineTemplate pipelineTemplate{};
					pipelineTemplate
						.setColorBlend(&Default::ColorBlending<std::array{Blending::Disable}>)
						.setShaderChain({&Shader::Vert::blitWithUV, &Shader::Frag::NFAA})
						.setStaticViewportAndScissor(pipeline.size().x, pipeline.size().y);

					pipeline.createPipeline(pipelineTemplate);
				};
			});

			processor.renderProcedure.resize(size);

			processor.createFramebuffer([](FramebufferLocal&){
				//using externals
			});

			processor.commandRecorder = [](Graphic::PostProcessor& postProcessor){
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
}

void Assets::PostProcess::dispose(){}

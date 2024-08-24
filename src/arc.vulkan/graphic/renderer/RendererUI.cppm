module;

#include <vulkan/vulkan.h>

export module Graphic.RendererUI;

import Core.Vulkan.BatchData;
import Core.Vulkan.Context;
import Core.Vulkan.RenderProcedure;
import Core.Vulkan.Buffer.FrameBuffer;
import Core.Vulkan.Attachment;
import Core.Vulkan.Uniform;

import Core.Vulkan.Comp;

import Core.Vulkan.Preinstall;

import Assets.Graphic;
import Graphic.Color;
import Graphic.Batch;

import std;

export namespace Graphic {

    struct Vertex_UI {
        Geom::Vec2 position{};
        Core::Vulkan::Util::TextureIndex textureParam{};
        Graphic::Color color{};
        Geom::Vec2 texCoord{};
    };

    struct UniformUI {
        Core::Vulkan::UniformMatrix3D proj{};

    };


    using UIVertBindInfo = Core::Vulkan::Util::VertexBindInfo<Vertex_UI, 0, 0, VK_VERTEX_INPUT_RATE_VERTEX,
        std::pair{&Vertex_UI::position, VK_FORMAT_R32G32_SFLOAT},
        std::pair{&Vertex_UI::textureParam, VK_FORMAT_R8G8B8A8_UINT},
        std::pair{&Vertex_UI::texCoord, VK_FORMAT_R32G32_SFLOAT},
        std::pair{&Vertex_UI::color, VK_FORMAT_R32G32B32A32_SFLOAT}
    >;

    struct RendererUI {
        //[base, def, light]
        static constexpr std::size_t AttachmentCount = 3;

        Batch batch{};
        
        Core::Vulkan::BatchData batchData{};

        Core::Vulkan::RenderProcedure batchDrawPass{};
        Core::Vulkan::RenderProcedure mergeDrawPass{};

        Core::Vulkan::FramebufferLocal batchFramebuffer{};
        Core::Vulkan::FramebufferLocal mergeFrameBuffer{};

        [[nodiscard]] const Core::Vulkan::Context& context() const {return *batch.context;}

        [[nodiscard]] RendererUI() = default;

        [[nodiscard]] explicit RendererUI(const Core::Vulkan::Context &context) :
            batch{context, sizeof(Vertex_UI)},
            batchData{batch.getBatchData()} {}

        Geom::USize2 size{};

        void resize(const Geom::USize2 size2) {
            if(this->size == size2)return;

            this->size = size2;
            batchDrawPass.resize(size2);
            mergeDrawPass.resize(size2);

            auto cmd = batch.obtainTransientCommand();
            batchFramebuffer.resize(size2, cmd);

            for(std::size_t i = 0; i < AttachmentCount; ++i) {
                mergeFrameBuffer.loadExternalImageView({{i, batchFramebuffer.atLocal(i).getView()}});
            }

            mergeFrameBuffer.resize(size2, cmd);
            cmd = {};
        }

        Core::Vulkan::Attachment& mergedAttachment() {
            return mergeFrameBuffer.localAttachments.back();
        }

        void initRenderPass(Geom::USize2 size2) {
            this->size = size2;

            initPipeline();
            createFrameBuffers();
        }
        
        void initPipeline() {
            using namespace Core::Vulkan;

            /*batchPass:*/ {
                batchDrawPass.init(context());

                batchDrawPass.createRenderPass([](RenderPass& renderPass){
                    const auto ColorAttachmentDesc = VkAttachmentDescription{}
                        | AttachmentDesc::Default
                        | AttachmentDesc::Stencil_DontCare
                        | AttachmentDesc::ReusedColorAttachment
                        | AttachmentDesc::ReadAndWrite;


                    for(std::size_t i = 0; i < AttachmentCount; ++i) {
                        renderPass.pushAttachment(ColorAttachmentDesc);
                    }

                    renderPass.pushSubpass([](RenderPass::SubpassData& subpass){
                        subpass.addDependency(VkSubpassDependency{
                                .dstSubpass = 0,
                                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                            } | Subpass::Dependency::External);

                        for(std::size_t i = 0; i < AttachmentCount; ++i) {
                            subpass.addOutput(i);
                        }
                    });
                });

                batchDrawPass.pushAndInitPipeline([](RenderProcedure::PipelineData& pipeline){
                    pipeline.createDescriptorLayout([](DescriptorSetLayout& layout){
                        layout.builder.push_seq(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
                        layout.builder.push_seq(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (8));
                    });

                    pipeline.createPipelineLayout();

                    pipeline.createDescriptorSet(1);
                    pipeline.addUniformBuffer(sizeof(UniformUI));

                    pipeline.builder = [](RenderProcedure::PipelineData& pipeline){
                        PipelineTemplate pipelineTemplate{};
                        pipelineTemplate
                            .useDefaultFixedStages()
                            .setColorBlend(&Default::ColorBlending<std::array{
                                Blending::ScaledAlphaBlend,
                                Blending::ScaledAlphaBlend,
                                Blending::ScaledAlphaBlend,
                            }>)
                            .setVertexInputInfo<UIVertBindInfo>()
                            .setShaderChain({&Assets::Shader::Vert::uiBatch, &Assets::Shader::Frag::uiBatch})
                            .setStaticViewport(float(pipeline.size().x), float(pipeline.size().y))
                            .setDynamicStates({VK_DYNAMIC_STATE_SCISSOR});

                        pipeline.createPipeline(pipelineTemplate);
                    };
                });

                batchDrawPass.resize(size);
            }

            /*mergePass:*/ {
                mergeDrawPass.init(context());

                mergeDrawPass.createRenderPass([](RenderPass& renderPass){
                    const auto ColorAttachmentDesc = VkAttachmentDescription{}
                        | AttachmentDesc::Default
                        | AttachmentDesc::Stencil_DontCare
                        | AttachmentDesc::ReusedColorAttachment
                        | AttachmentDesc::ReadAndWrite;

                    for(std::size_t i = 0; i < AttachmentCount; ++i) {
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

                        for(std::size_t i = 0; i < AttachmentCount; ++i) {
                            subpass.addInput(i);
                        }

                        subpass.addInput(AttachmentCount + 1);

                        subpass.addOutput(AttachmentCount + 0);
                        subpass.addOutput(AttachmentCount + 1);
                    });
                });

                mergeDrawPass.pushAndInitPipeline([](RenderProcedure::PipelineData& pipeline){
                    pipeline.createDescriptorLayout([](DescriptorSetLayout& layout){
                        layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
                        layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
                        layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
                        layout.builder.push_seq(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
                    });

                    pipeline.createPipelineLayout();

                    pipeline.builder = [](RenderProcedure::PipelineData& pipeline){
                        PipelineTemplate pipelineTemplate{};
                        pipelineTemplate
                            .useDefaultFixedStages()
                            .setColorBlend(&Default::ColorBlending<std::array{
                                    Blending::ScaledAlphaBlend,
                                    Blending::ScaledAlphaBlend,
                                }>)
                            .setVertexInputInfo<Util::EmptyVertexBind>()
                            .setShaderChain({&Assets::Shader::Vert::blitSingle, &Assets::Shader::Frag::uiMerge})
                            .setStaticViewportAndScissor(pipeline.size().x, pipeline.size().y);

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

            for(std::size_t i = 0; i < AttachmentCount; ++i) {
                ColorAttachment colorAttachment{context().physicalDevice, context().device};
                colorAttachment.create(size, batch.obtainTransientCommand(),
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                );
                batchFramebuffer.pushCapturedAttachments(std::move(colorAttachment));
            }

            batchFramebuffer.loadCapturedAttachments(AttachmentCount);
            batchFramebuffer.create();

            for(std::size_t i = 0; i < 2; ++i) {
                ColorAttachment colorAttachment{context().physicalDevice, context().device};
                colorAttachment.create(size, batch.obtainTransientCommand(),
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                );
                mergeFrameBuffer.addCapturedAttachments(AttachmentCount + i, std::move(colorAttachment));
            }

            mergeFrameBuffer.loadCapturedAttachments(0);
            for(std::size_t i = 0; i < AttachmentCount; ++i) {
                mergeFrameBuffer.loadExternalImageView({{i, batchFramebuffer.atLocal(i).getView()}});
            }
            mergeFrameBuffer.create();

        }
    };
}



module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.RenderPassGroup;

import Core.Vulkan.Buffer.FrameBuffer;
import Core.Vulkan.Buffer.UniformBuffer;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Attachment;
import Core.Vulkan.Pipeline;
import Core.Vulkan.PipelineLayout;
import Core.Vulkan.DescriptorSet;
import Core.Vulkan.Dependency;
import Core.Vulkan.Context;
import Core.Vulkan.Concepts;

import Core.Vulkan.CommandPool;
import Core.Vulkan.RenderPass;

import Geom.Vector2D;

import std;

export namespace Core::Vulkan{

	struct RenderPassGroup{
		struct PipelineData{
			// VkSubpassContents contents;
			std::uint32_t index{};
			RenderPassGroup* group{};

			PipelineLayout layout{};
			GraphicPipeline pipeline{};

			DescriptorSetLayout descriptorSetLayout{};

			//TODO multiple set support
			DescriptorSetPool descriptorSetPool{};
			std::vector<DescriptorSet> descriptorSets{};
			// DescriptorSet descriptorSet{};

			std::vector<UniformBuffer> uniformBuffers{};


			template <std::regular_invocable<DescriptorSetLayout&> Func>
			void setDescriptorLayout(Func&& func){
				descriptorSetLayout = DescriptorSetLayout{group->context->device, std::forward<Func>(func)};
			}

			void setPushRanges() = delete;

			void setPipelineLayout(VkPipelineCreateFlags flags = 0){
				layout = PipelineLayout{group->context->device, flags, descriptorSetLayout.asSeq(), {}};
			}

			void createPipeline(VkDevice device, VkRenderPass renderPass, PipelineTemplate& pipelineTemplate){
				pipeline = GraphicPipeline{device, pipelineTemplate, layout, renderPass};
			}

			void createDescriptorSet(std::uint32_t size){
				descriptorSetPool = DescriptorSetPool{group->context->device, descriptorSetLayout, size};
				descriptorSets = descriptorSetPool.obtain(size);
			}

			void createPipeline(PipelineTemplate& pipelineTemplate){
				pipeline =	GraphicPipeline{group->context->device, pipelineTemplate, layout, group->renderPass, index};
			}
		};

		Context* context{};
		RenderPass renderPass{};
		Geom::USize2 size{};

		std::vector<PipelineData> pipelines{};
		std::function<void(RenderPassGroup&)> rebuildPipelineFunc{};

		//init
		//set renderpass params
		//set local attachments
		//insert external attachments

		//generate framebuffer
		//generate pipeline
		//command record

		void init(Context& context){
			this->context = &context;

			renderPass = RenderPass{context.device};
		}

		template <std::regular_invocable<RenderPass&> Func>
		void createRenderPass(Func&& func){
			func(renderPass);

			renderPass.createRenderPass();

			// usedPipelines.resize(renderPass.subpassSize());
		}


		template<
			ContigiousRange<VkDescriptorSetLayout> R1,
			ContigiousRange<VkPushConstantRange> R2 = EmptyRange<VkPushConstantRange>>
		void pushPipeline(PipelineTemplate& pipelineTemplate, R1&& descriptorSetLayouts, R2&& constantRange = {}){
			PipelineData& group = pushAndInitPipeline();

			// static_assert(false, "TODO");

			// group.setLayout(context->device, descriptorSetLayouts, constantRange);

			group.createPipeline(context->device, renderPass, pipelineTemplate);
		}

		decltype(auto) pushAndInitPipeline(){
			return pipelines.emplace_back(pipelines.size(), this);
		}

		template <std::regular_invocable<PipelineData&> InitFunc>
		void pushAndInitPipeline(InitFunc&& func){
			func(pushAndInitPipeline());
		}

		void resize(const Geom::USize2 size){
			this->size = size;
			if(rebuildPipelineFunc){
				pipelines.clear();
				rebuildPipelineFunc(*this);
			}
		}

		void recordCommandTo(){

		}

		//
		// [[nodiscard]] std::size_t getRequiredSubpassSize_() const{
		// 	auto view = renderPass.subpasses
		// 		| std::ranges::views::transform(&RenderPass::SubpassData::attachment)
		// 		| std::ranges::views::transform(&RenderPass::AttachmentReference::getMaxIndex);
		// 	auto rst = std::ranges::max(view);
		//
		// 	return rst;
		// }
	};
}

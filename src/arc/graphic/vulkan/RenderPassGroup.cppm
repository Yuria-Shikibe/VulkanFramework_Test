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

			void overrideIndex(const std::uint32_t index){
				this->index = index;
			}

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
				pipeline = GraphicPipeline{group->context->device, pipelineTemplate, layout, group->renderPass, index};
			}


			void addUniformBuffer(const std::size_t size){
				uniformBuffers.push_back(UniformBuffer{group->context->physicalDevice, group->context->device, size});
			}

			std::function<void(PipelineData&)> builder{};
		};

		Context* context{};
		RenderPass renderPass{};
		Geom::USize2 size{};

		std::vector<PipelineData> pipelinesLocal{};

		std::vector<std::pair<std::uint32_t, VkPipeline>> pipelinesExternal{};
		std::vector<VkPipeline> pipelines{};

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

			pipelines.resize(renderPass.subpassSize());
		}



		decltype(auto) pushAndInitPipeline(){
			return pipelinesLocal.emplace_back(pipelinesLocal.size(), this);
		}

		template <std::regular_invocable<PipelineData&> InitFunc>
		void pushAndInitPipeline(InitFunc&& func){
			func(pushAndInitPipeline());
		}

		void resize(const Geom::USize2 size){
			this->size = size;

			for (auto& pipeline : pipelinesLocal){
				if(pipeline.builder){
					pipeline.builder(pipeline);
				}
			}

			loadPipelines();
		}

		[[nodiscard]] VkPipeline pipelineAt(const std::size_t index) const{
			return pipelines[index];
		}

		[[nodiscard]] VkPipeline operator[](const std::size_t index) const{
			return pipelineAt(index);
		}

		void loadExternalPipelines(const std::initializer_list<decltype(pipelinesExternal)::value_type> pipelines){
			pipelinesExternal = pipelines;
		}

		void loadPipelines(){
			std::map<std::uint32_t, VkPipeline> checkedPipelines{};

			for (const auto& pipeline : pipelinesLocal){
				auto [r, suc] = checkedPipelines.try_emplace(pipeline.index, pipeline.pipeline.get());
				if(!suc){
					throw std::runtime_error("Duplicated Index");
				}
			}

			for (auto [index, pipeline] : pipelinesExternal){
				auto [r, suc] = checkedPipelines.try_emplace(index, pipeline);
				if(!suc){
					throw std::runtime_error("Duplicated Index");
				}
			}

			if(checkedPipelines.empty()){
				throw std::runtime_error("No valid pipeline");
			}

			const auto max = checkedPipelines.rbegin()->first;

			pipelines.resize(max + 1);

			for (auto [index, p] : checkedPipelines){
				pipelines[index] = p;
			}

			for (const auto & vkPipeline : pipelines){
				if(!vkPipeline)throw std::runtime_error("VkPipeline is NULL");
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

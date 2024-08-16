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

	struct RenderProcedure{
		struct PipelineData{
			// std::uint32_t index{};
			RenderProcedure* group{};

			std::vector<std::pair<std::uint32_t, GraphicPipeline>> targetIndices{};

			DescriptorSetLayout descriptorSetLayout{};
			ConstantLayout constantLayout{};
			PipelineLayout layout{};

			// GraphicPipeline pipeline{};

			DescriptorSetPool descriptorSetPool{};
			std::vector<DescriptorSet> descriptorSets{};
			std::vector<UniformBuffer> uniformBuffers{};

			std::function<void(PipelineData&)> builder{};

			[[nodiscard]] auto size() const{
				return group->size;
			}

			// void overrideIndex(const std::uint32_t index){
			// 	this->index = index;
			// }

			template <ContigiousRange<std::uint32_t> Rng = std::initializer_list<std::uint32_t>>
			void addTarget(Rng&& list){
				for (std::uint32_t index : list){
					targetIndices.push_back({index, {}});
				}
			}


			template <ContigiousRange<std::uint32_t> Rng = std::initializer_list<std::uint32_t>>
			void setTarget(Rng&& list){
				targetIndices.clear();
				for (std::uint32_t index : list){
					targetIndices.push_back({index, {}});
				}
			}

			template <std::regular_invocable<DescriptorSetLayout&> Func>
			void setDescriptorLayout(Func&& func){
				descriptorSetLayout = DescriptorSetLayout{group->context->device, std::forward<Func>(func)};
			}

			void pushConstant(const VkPushConstantRange& constantRange){
				constantLayout.push(constantRange);
			}

			/**
			 * @brief Call after descriptors and constants have been set
			 */
			void setPipelineLayout(VkPipelineCreateFlags flags = 0){
				layout = PipelineLayout{group->context->device, flags, descriptorSetLayout.asSeq(), constantLayout.constants};
			}

			void createDescriptorSet(std::uint32_t size){
				descriptorSetPool = DescriptorSetPool{group->context->device, descriptorSetLayout, size};
				descriptorSets = descriptorSetPool.obtain(size);
			}

			void createPipeline(PipelineTemplate& pipelineTemplate){
				for (auto && [index, pipeline] : targetIndices){
					pipeline = GraphicPipeline{group->context->device, pipelineTemplate, layout, group->renderPass, index};
				}
			}

			void addUniformBuffer(const std::size_t size){
				uniformBuffers.push_back(UniformBuffer{group->context->physicalDevice, group->context->device, size});
			}
			template <typename T>
			void addUniformBuffer(){
				addUniformBuffer(sizeof(T));
			}

			template <ContigiousRange<std::uint32_t> Rng = std::initializer_list<std::uint32_t>>
			void bindDescriptorTo(
				VkCommandBuffer commandBuffer,
				VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				std::uint32_t firstSet = 0, Rng&& dynamicOffset = {}){
				::vkCmdBindDescriptorSets(
					commandBuffer, bindPoint,
					layout, firstSet,
					descriptorSets.size(), reinterpret_cast<const VkDescriptorSet*>(descriptorSets.data()),
					std::ranges::size(dynamicOffset), std::ranges::data(dynamicOffset)
				);
			}

			template <typename T>
			void bindConstantTo(
				VkCommandBuffer commandBuffer,
				VkShaderStageFlags stageFlags,
				const T& data, std::uint32_t offset = 0){
				::vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, sizeof(T), static_cast<const void*>(&data));
			}

			template <typename ...T>
			void bindConstantToSeq(VkCommandBuffer commandBuffer, const T& ...args){
#if DEBUG_CHECK
				if(sizeof...(args) > constantLayout.constants.size()){
					throw std::invalid_argument("Constant size out of bound");
				}
#endif

				[&, t = this]<std::size_t ...I>(std::index_sequence<I...>){
					(t->bindConstantTo(commandBuffer, constantLayout.constants[I].stageFlags, args, constantLayout.constants[I].offset), ...);
				}(std::index_sequence_for<T...>{});
			}
		};

		Context* context{};
		RenderPass renderPass{};
		Geom::USize2 size{};

		std::vector<PipelineData> pipelinesLocal{};

		using PipelinePlugin = std::pair<std::uint32_t, VkPipeline>;

		std::vector<PipelinePlugin> pipelinesExternal{};

		std::vector<std::pair<VkPipeline, PipelineData*>> pipelines{};

		void init(Context& context){
			this->context = &context;

			renderPass = RenderPass{context.device};
		}

		/**
		 * @brief Renderpass should be createable after func
		 */
		template <std::regular_invocable<RenderPass&> Func>
		void createRenderPass(Func&& initFunc){
			initFunc(renderPass);

			renderPass.createRenderPass();

			pipelines.resize(renderPass.subpassSize());
		}


		decltype(auto) pushAndInitPipeline(){
			auto& rst = pipelinesLocal.emplace_back(this);
			rst.setTarget({static_cast<std::uint32_t>(pipelinesLocal.size() - 1)});
			return rst;
		}

		template <std::regular_invocable<PipelineData&> InitFunc>
		void pushAndInitPipeline(InitFunc&& func){
			func(pushAndInitPipeline());
		}

		template <std::regular_invocable<PipelineData&> InitFunc>
		void addPipeline(InitFunc&& func){
			func(pipelinesLocal.emplace_back(this));
		}

		template <ContigiousRange<PipelinePlugin> Rng = std::initializer_list<PipelinePlugin>>
		void resize(const Geom::USize2 size, Rng&& pipelines = {}){
			this->size = size;

			for (auto& pipeline : pipelinesLocal){
				if(pipeline.builder){
					pipeline.builder(pipeline);
				}
			}

			this->loadExternalPipelines(std::forward<Rng>(pipelines));
			loadPipelines();
		}

		[[nodiscard]] VkPipeline at(const std::size_t index) const{
			return pipelines[index].first;
		}

		[[nodiscard]] decltype(auto) atLocal(const std::size_t index){return pipelinesLocal[index];}

		[[nodiscard]] decltype(auto) atLocal(const std::size_t index) const {return pipelinesLocal[index];}

		[[nodiscard]] decltype(auto) front(){return pipelinesLocal[0];}

		[[nodiscard]] decltype(auto) front() const {return pipelinesLocal[0];}

		[[nodiscard]] VkPipeline operator[](const std::size_t index) const{return at(index);}

		template <ContigiousRange<PipelinePlugin> Rng = std::initializer_list<PipelinePlugin>>
		void loadExternalPipelines(Rng&& pipelines){
			pipelinesExternal = {std::ranges::begin(pipelines), std::ranges::end(pipelines)};
		}

		void loadPipelines(){
			using Pair = std::pair<VkPipeline, PipelineData*>;
			std::map<std::uint32_t, Pair> checkedPipelines{};

			for(auto& pipelineData : pipelinesLocal){
				for(const auto& [index, pipeline] : pipelineData.targetIndices){
					auto [r, suc] =
						checkedPipelines.try_emplace(index, Pair{pipeline.get(), &pipelineData});

					if(!suc){
						throw std::runtime_error("Duplicated Index");
					}
				}
			}

			for (auto [index, pipeline] : pipelinesExternal){
				auto [r, suc] = checkedPipelines.try_emplace(index, Pair{pipeline, nullptr});
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

			if(std::ranges::any_of(pipelines | std::views::keys, std::not_fn(std::identity{}))){
				throw std::runtime_error("VkPipeline is NULL");
			}
		}

		//TODO make it as iterator??
		struct PipelineContext{
			RenderProcedure& renderProcedure;
			std::uint32_t currentIndex{};

			[[nodiscard]] PipelineData& data() const{
				if(auto p = renderProcedure.pipelines.at(currentIndex).second)return *p;

				throw std::runtime_error("No valid local pipeline");
			}

			[[nodiscard]] VkPipeline pipeline() const{
				return renderProcedure[currentIndex];
			}

			void next(VkCommandBuffer commandBuffer, VkSubpassContents contents){
				++currentIndex;

				if(currentIndex > renderProcedure.renderPass.subpasses.size()){
					throw std::runtime_error("Subpass index out of range");
				}

				vkCmdNextSubpass(commandBuffer, contents);
			}

			~PipelineContext() noexcept(false) {
				if((currentIndex + 1) != renderProcedure.renderPass.subpasses.size()){
					throw std::runtime_error("Subpass not finished");
				}
			}
		};

		PipelineContext getCmdContext(){
			return PipelineContext{*this};
		}

		[[nodiscard]] VkRenderPassBeginInfo getBeginInfo(VkFramebuffer framebuffer) const noexcept{
			return {
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.pNext = nullptr,
				.renderPass = renderPass,
				.framebuffer = framebuffer,
				.renderArea = {{}, std::bit_cast<VkExtent2D>(size)},
			};
		}
	};
}

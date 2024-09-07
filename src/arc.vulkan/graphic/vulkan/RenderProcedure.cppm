module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.RenderProcedure;

export import Core.Vulkan.Buffer.FrameBuffer;
export import Core.Vulkan.Buffer.UniformBuffer;
export import Core.Vulkan.Buffer.CommandBuffer;
export import Core.Vulkan.Attachment;
export import Core.Vulkan.Pipeline;
export import Core.Vulkan.PipelineLayout;
export import Core.Vulkan.DescriptorSet;
export import ext.handle_wrapper;
export import Core.Vulkan.CommandPool;
export import Core.Vulkan.RenderPass;
export import Core.Vulkan.DescriptorBuffer;



import Core.Vulkan.Context;
import Core.Vulkan.Concepts;

import Geom.Vector2D;

import std;

export namespace Core::Vulkan{
	struct RenderProcedure;

	struct BasicPipelineData{
		const Context* context{};

		Geom::USize2 size{};

		DescriptorSetLayout descriptorSetLayout{};
		ConstantLayout constantLayout{};

		PipelineLayout layout{};

		[[nodiscard]] BasicPipelineData() = default;

		[[nodiscard]] explicit BasicPipelineData(const Context* context, const Geom::USize2 size = {})
			: context{context},
			  size{size}{}

		[[nodiscard]] Geom::USize2 getSize() const{
			return size;
		}

		template <std::regular_invocable<DescriptorSetLayout&> Func>
		void createDescriptorLayout(Func&& func){
			descriptorSetLayout = DescriptorSetLayout{context->device, std::forward<Func>(func)};
		}

		void pushConstant(const VkPushConstantRange& constantRange){
			constantLayout.push(constantRange);
		}

		/**
		 * @brief Call after descriptors and constants have been set
		 */
		template <std::ranges::range Range = std::initializer_list<VkDescriptorSetLayout>>
			requires (std::convertible_to<std::ranges::range_value_t<Range>, VkDescriptorSetLayout>)
		void createPipelineLayout(const VkPipelineCreateFlags flags = 0, Range&& appendLayouts = {}){
			std::vector<VkDescriptorSetLayout> ls{};
			ls.push_back(descriptorSetLayout.get());
			ls.append_range(appendLayouts);
			layout = PipelineLayout{context->device, flags, ls, constantLayout.constants};
		}
	};

	struct SinglePipelineData : BasicPipelineData{
		using BasicPipelineData::BasicPipelineData;
		Pipeline pipeline{};

		void createPipeline(PipelineTemplate& pipelineTemplate){
			pipeline = Pipeline{context->device, pipelineTemplate, layout, nullptr, 0};
		}
		void createComputePipeline(VkPipelineCreateFlags flags, const VkPipelineShaderStageCreateInfo& stageCreateInfo){
			pipeline = Pipeline{context->device, layout, flags, stageCreateInfo};
		}

		void bind(VkCommandBuffer commandBuffer, const VkPipelineBindPoint bindPoint) const{
			vkCmdBindPipeline(commandBuffer, bindPoint, pipeline);
		}

		void createComputePipeline(VkPipelineCreateFlags flags, VkShaderModule shaderModule, const char* entryName = "main"){
			createComputePipeline(flags, {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.stage = VK_SHADER_STAGE_COMPUTE_BIT,
					.module = shaderModule,
					.pName = entryName,
					.pSpecializationInfo = nullptr
				});
		}
	};

	struct PipelineData : BasicPipelineData{
		using BasicPipelineData::BasicPipelineData;

		VkRenderPass renderPass{};
		std::vector<std::pair<std::uint32_t, Pipeline>> pipes{};

		DescriptorSetPool descriptorSetPool{};
		std::vector<DescriptorBuffer> descriptorBuffers{};

		std::vector<DescriptorSet> descriptorSets{};
		std::vector<UniformBuffer> uniformBuffers{};

		std::function<void(PipelineData&)> builder{};

		void bind(VkCommandBuffer commandBuffer, const VkPipelineBindPoint bindPoint, const std::size_t index = 0) const{
			vkCmdBindPipeline(commandBuffer, bindPoint, pipes[index].second);
		}

		[[nodiscard]] Geom::USize2 getSize() const{
			return size;
		}

		void resize(const Geom::USize2 size, VkRenderPass renderPass = nullptr){
			this->size = size;
			this->renderPass = renderPass;
			if(builder){
				builder(*this);
			}
		}

		template <RangeOf<std::uint32_t> Rng = std::initializer_list<std::uint32_t>>
		void addTarget(Rng&& list){
			for(std::uint32_t index : list){
				pipes.push_back({index, {}});
			}
		}

		template <RangeOf<std::uint32_t> Rng = std::initializer_list<std::uint32_t>>
		void setTarget(Rng&& list){
			pipes.clear();
			for(std::uint32_t index : list){
				pipes.push_back({index, {}});
			}
		}

		void createDescriptorSet(const std::uint32_t size){
			descriptorSetPool = DescriptorSetPool{context->device, descriptorSetLayout, size};
			descriptorSets = descriptorSetPool.obtain(size);
		}

		void createDescriptorBuffers(const std::uint32_t size){
			descriptorBuffers.reserve(size);
			for(std::size_t i = 0; i < size; ++i){
				descriptorBuffers.push_back(DescriptorBuffer{
					context->physicalDevice, context->device, descriptorSetLayout, descriptorSetLayout.size()
				});
			}
		}

		void createPipeline(PipelineTemplate& pipelineTemplate){
			for(auto&& [index, pipeline] : pipes){
				pipeline = Pipeline{context->device, pipelineTemplate, layout, renderPass, index};
			}
		}

		void createComputePipeline(VkPipelineCreateFlags flags, const VkPipelineShaderStageCreateInfo& stageCreateInfo){
			for(auto& pipeline : pipes | std::views::values){
				pipeline = Pipeline{context->device, layout, flags, stageCreateInfo};
			}
		}

		void createComputePipeline(VkPipelineCreateFlags flags, VkShaderModule shaderModule, const char* entryName = "main"){
			createComputePipeline(flags, {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.stage = VK_SHADER_STAGE_COMPUTE_BIT,
					.module = shaderModule,
					.pName = entryName,
					.pSpecializationInfo = nullptr
				});
		}

		void addUniformBuffer(const std::size_t size, const VkBufferUsageFlags otherUsages = 0){
			uniformBuffers.push_back(UniformBuffer{context->physicalDevice, context->device, size, otherUsages});
		}

		template <typename T>
		void addUniformBuffer(){
			addUniformBuffer(sizeof(T));
		}

		template <ContigiousRange<std::uint32_t> Rng = std::initializer_list<std::uint32_t>>
		void bindDescriptorTo(
			VkCommandBuffer commandBuffer,
			VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			std::uint32_t firstSet = 0, Rng&& dynamicOffset = {}) const{
			::vkCmdBindDescriptorSets(
				commandBuffer, bindPoint,
				layout, firstSet,
				descriptorSets.size(), reinterpret_cast<const VkDescriptorSet*>(descriptorSets.data()),
				std::ranges::size(dynamicOffset), std::ranges::data(dynamicOffset)
			);
		}

		void bindDescriptorTo(
			VkCommandBuffer commandBuffer, const std::size_t index,
			VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS) const{
			::vkCmdBindDescriptorSets(
				commandBuffer, bindPoint,
				layout, 0,
				1, descriptorSets.at(index).as_data(),
				0, nullptr
			);
		}

		template <typename T>
		void bindConstantTo(
			VkCommandBuffer commandBuffer,
			VkShaderStageFlags stageFlags,
			const T& data, std::uint32_t offset = 0){
			::vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, sizeof(T), static_cast<const void*>(&data));
		}

		template <typename... T>
		void bindConstantToSeq(VkCommandBuffer commandBuffer, const T&... args){
#if DEBUG_CHECK
			if(sizeof...(args) > constantLayout.constants.size()){
				throw std::invalid_argument("Constant size out of bound");
			}
#endif

			[&, t = this]<std::size_t ... I>(std::index_sequence<I...>){
				(t->bindConstantTo(commandBuffer, constantLayout.constants[I].stageFlags, args,
				                   constantLayout.constants[I].offset), ...);
			}(std::index_sequence_for<T...>{});
		}
	};


	struct RenderProcedure{
		const Context* context{};
		RenderPass renderPass{};
		Geom::USize2 size{};

		std::vector<PipelineData> pipelinesLocal{};

		using PipelinePlugin = std::pair<std::uint32_t, VkPipeline>;

		std::vector<PipelinePlugin> pipelinesExternal{};

		std::vector<std::pair<VkPipeline, PipelineData*>> pipelines{};

		[[nodiscard]] RenderProcedure() = default;

		[[nodiscard]] explicit RenderProcedure(const Context& context) : context{&context}{
			renderPass = RenderPass{context.device};
		}

		void init(const Context& context){
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
			auto& rst = pipelinesLocal.emplace_back(context, size);
			rst.setTarget({static_cast<std::uint32_t>(pipelinesLocal.size() - 1)});
			return rst;
		}

		template <std::regular_invocable<PipelineData&> InitFunc>
		void pushAndInitPipeline(InitFunc&& func){
			func(pushAndInitPipeline());
		}

		template <std::regular_invocable<PipelineData&> InitFunc>
		void addPipeline(InitFunc&& func){
			func(pipelinesLocal.emplace_back(context, size));
		}

		template <ContigiousRange<PipelinePlugin> Rng = std::initializer_list<PipelinePlugin>>
		void resize(const Geom::USize2 size, Rng&& pipelines = {}){
			this->size = size;

			for(auto& pipeline : pipelinesLocal){
				pipeline.resize(size, renderPass);
			}

			this->loadExternalPipelines(std::forward<Rng>(pipelines));
			loadPipelines();
		}

		[[nodiscard]] VkPipeline at(const std::size_t index) const{
			return pipelines[index].first;
		}

		[[nodiscard]] decltype(auto) atLocal(const std::size_t index){ return pipelinesLocal[index]; }

		[[nodiscard]] decltype(auto) atLocal(const std::size_t index) const{ return pipelinesLocal[index]; }

		[[nodiscard]] decltype(auto) front(){ return pipelinesLocal[0]; }

		[[nodiscard]] decltype(auto) front() const{ return pipelinesLocal[0]; }

		[[nodiscard]] VkPipeline operator[](const std::size_t index) const{ return at(index); }

		template <ContigiousRange<PipelinePlugin> Rng = std::initializer_list<PipelinePlugin>>
		void loadExternalPipelines(Rng&& pipelines){
			pipelinesExternal = {std::ranges::begin(pipelines), std::ranges::end(pipelines)};
		}

		void loadPipelines(){
			using Pair = std::pair<VkPipeline, PipelineData*>;
			std::map<std::uint32_t, Pair> checkedPipelines{};

			for(auto& pipelineData : pipelinesLocal){
				for(const auto& [index, pipeline] : pipelineData.pipes){
					auto [r, suc] =
						checkedPipelines.try_emplace(index, Pair{pipeline.get(), &pipelineData});

					if(!suc){
						throw std::runtime_error("Duplicated Index");
					}
				}
			}

			for(auto [index, pipeline] : pipelinesExternal){
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

			for(auto [index, p] : checkedPipelines){
				pipelines[index] = p;
			}

			if(std::ranges::any_of(pipelines | std::views::keys, std::not_fn(std::identity{}))){
				throw std::runtime_error("VkPipeline is NULL");
			}
		}

		//TODO make it as iterator??
		struct PipelineContext{
			RenderProcedure& renderProcedure;
			VkCommandBuffer commandBuffer{};
			std::uint32_t currentIndex{};
			VkPipelineBindPoint bindPoint{};

			[[nodiscard]] PipelineContext(
				RenderProcedure& renderProcedure,
				VkCommandBuffer commandBuffer,
				VkPipelineBindPoint bindPoint
			)
				: renderProcedure{renderProcedure},
				  commandBuffer{commandBuffer}, bindPoint{bindPoint}{
				vkCmdBindPipeline(commandBuffer, bindPoint, currentPipeline());
			}

			[[nodiscard]] PipelineData& data() const{
				if(auto p = renderProcedure.pipelines.at(currentIndex).second) return *p;

				throw std::runtime_error("No valid local pipeline");
			}

			[[nodiscard]] VkPipeline currentPipeline() const{
				return renderProcedure[currentIndex];
			}

			void next(VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE){
				++currentIndex;

				if(currentIndex == renderProcedure.renderPass.subpasses.size()) return;

				if(currentIndex > renderProcedure.renderPass.subpasses.size()){
					throw std::runtime_error("Subpass index out of range");
				}

				vkCmdNextSubpass(commandBuffer, contents);
				vkCmdBindPipeline(commandBuffer, bindPoint, currentPipeline());
			}

			~PipelineContext() noexcept(false){
				if((currentIndex + 1) < renderProcedure.renderPass.subpasses.size()){
					throw std::runtime_error("Subpass not finished");
				}
			}
		};

		PipelineContext startCmdContext(VkCommandBuffer commandBuffer,
		                                VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS){
			return PipelineContext{*this, commandBuffer, bindPoint};
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

module : private;

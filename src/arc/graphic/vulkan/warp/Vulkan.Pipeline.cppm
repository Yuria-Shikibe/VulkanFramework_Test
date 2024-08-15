module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Pipeline;

import Core.Vulkan.Concepts;
import Core.Vulkan.Shader;
import Core.Vulkan.Dependency;
import Core.Vulkan.Preinstall;
import Core.Vulkan.PipelineLayout;
import std;

export namespace Core::Vulkan{
	struct PipelineTemplate : VkGraphicsPipelineCreateInfo{
	private:
		struct MutexWrapper{
			std::mutex mutex{};
			operator std::mutex&() noexcept{return mutex;}
			[[nodiscard]] MutexWrapper() = default;
			MutexWrapper(const MutexWrapper&) {}
			MutexWrapper(MutexWrapper&&) noexcept {}
			MutexWrapper& operator=(const MutexWrapper&){return *this;}
			MutexWrapper& operator=(MutexWrapper&&) noexcept{return *this;}
		} mutex{}; //TODO

	public:
		ShaderChain shaderChain{};

		std::vector<VkDynamicState> dynamicStates{};
		VkPipelineDynamicStateCreateInfo dynamicStateInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		VkPipelineViewportStateCreateInfo viewportState{};

		std::uint32_t dynamicViewportSize{};
		std::vector<VkViewport> staticViewports{};
		std::vector<VkRect2D> staticScissors{};

		[[nodiscard]] std::uint32_t viewportSize() const{
			return dynamicViewportSize > 0 ? dynamicViewportSize : staticViewports.size();
		}

		[[nodiscard]] PipelineTemplate() : VkGraphicsPipelineCreateInfo{
				VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
			}{}

		PipelineTemplate& useDefaultFixedStages(){
			viewportState = Default::DynamicViewportState<1>;

			pInputAssemblyState = &Default::InputAssembly;
			pRasterizationState = &Default::Rasterizer;
			pMultisampleState = &Default::Multisampling;
			pColorBlendState = &Default::ColorBlending<std::array{Blending::AlphaBlend}>;

			return *this;
		}

		PipelineTemplate& setColorBlend(const VkPipelineColorBlendStateCreateInfo* info){
			pColorBlendState = info;
			return *this;
		}

		PipelineTemplate& setStaticViewport(const RangeOf<VkViewport> auto& viewports){
			staticViewports = std::vector<VkViewport>{std::ranges::begin(viewports), std::ranges::end(viewports)};

			viewportState.viewportCount = staticViewports.size();
			viewportState.pViewports = staticViewports.data();
			dynamicViewportSize = 0;

			return *this;
		}

		PipelineTemplate& setStaticViewport(const VkViewport& viewport){
			return setStaticViewport(std::array{viewport});
		}

		PipelineTemplate& setStaticViewport(const float w, const float h){
			return setStaticViewport(std::array{VkViewport{
				.x = 0,
				.y = 0,
				.width = w,
				.height = h,
				.minDepth = 0.f,
				.maxDepth = 1.f
			}});
		}

		PipelineTemplate& setStaticScissors(const RangeOf<VkRect2D> auto& viewports){
			staticScissors = std::vector<VkRect2D>{std::ranges::begin(viewports), std::ranges::end(viewports)};

			viewportState.scissorCount = staticScissors.size();
			viewportState.pScissors = staticScissors.data();
			dynamicViewportSize = 0;

			return *this;
		}

		PipelineTemplate& setStaticScissors(const VkRect2D& viewport){
			return setStaticScissors(std::array{viewport});
		}

		PipelineTemplate& setDynamicViewportCount(const std::uint32_t size){
			dynamicViewportSize = size;
			viewportState.scissorCount = viewportState.viewportCount = size;
			viewportState.pScissors = nullptr;
			viewportState.pViewports = nullptr;

			return *this;
		}

		PipelineTemplate& autoFetchStaticScissors(){
			staticScissors.resize(staticViewports.size());

			for (const auto& [index, staticScissor] : staticScissors | std::views::enumerate){
				staticScissor.offset = {};
				staticScissor.extent.width = static_cast<std::uint32_t>(std::round(staticViewports[index].width));
				staticScissor.extent.height = static_cast<std::uint32_t>(std::round(staticViewports[index].height));
			}
			dynamicViewportSize = 0;

			return *this;
		}

		PipelineTemplate& setShaderChain(ShaderChain&& shaderChain){
			this->shaderChain = std::move(shaderChain);

			stageCount = shaderChain.size();
			pStages = shaderChain.data();

			return *this;
		}

		PipelineTemplate& setDynamicStates(const std::initializer_list<VkDynamicState> states){
			this->dynamicStates = states;
			dynamicStateInfo = Util::createDynamicState(dynamicStates);

			return *this;
		}

		template <Util::VertexInfo Prov>
		PipelineTemplate& setVertexInputInfo(){
			vertexInputInfo = Util::getVertexInfo<Prov>();

			return *this;
		}

		PipelineTemplate& setVertexInputInfo(const VkPipelineVertexInputStateCreateInfo& info){
			vertexInputInfo = info;

			return *this;
		}

		[[nodiscard]] auto create(VkDevice device,
			VkPipelineLayout layout, VkRenderPass renderPass, std::uint32_t subpassIndex = 0){
			std::lock_guard lg{mutex.mutex};

			this->layout = layout;
			this->renderPass = renderPass;
			this->subpass = subpassIndex;

			this->stageCount = shaderChain.size();
			this->pStages = shaderChain.data();

			this->pVertexInputState = &vertexInputInfo;

			this->pDynamicState = &dynamicStateInfo;

			this->pViewportState = &viewportState;

			VkPipeline pipeline{};
			auto rst = vkCreateGraphicsPipelines(device, nullptr, 1, this, nullptr, &pipeline);
			return std::make_pair(pipeline, rst);
		}
	};

	struct GraphicPipeline : Wrapper<VkPipeline>{
		DeviceDependency device{};

		[[nodiscard]] GraphicPipeline() = default;

		[[nodiscard]] GraphicPipeline(VkDevice device, VkPipeline pipeline) : Wrapper{pipeline}, device{device}{}

		[[nodiscard]] GraphicPipeline(
			VkDevice device,
			PipelineTemplate& pipelineTemplate,
			VkPipelineLayout layout, VkRenderPass renderPass, std::uint32_t subpassIndex = 0) : device{device}{
			auto [p, rst] = pipelineTemplate.create(device, layout, renderPass, subpassIndex);

			if(rst){
				throw std::runtime_error("Failed to create pipeline");
			}

			handle = p;
		}

		~GraphicPipeline(){
			if(device)vkDestroyPipeline(device, handle, nullptr);
		}

		GraphicPipeline(const GraphicPipeline& other) = delete;

		GraphicPipeline(GraphicPipeline&& other) noexcept = default;

		GraphicPipeline& operator=(const GraphicPipeline& other) = delete;

		GraphicPipeline& operator=(GraphicPipeline&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroyPipeline(device, handle, nullptr);
			Wrapper<VkPipeline>::operator =(std::move(other));
			device = std::move(other.device);
			return *this;
		}
	};
}

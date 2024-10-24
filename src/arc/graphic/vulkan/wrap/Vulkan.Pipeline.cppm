module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Pipeline;

export import Core.Vulkan.PipelineLayout;
import Core.Vulkan.Concepts;
import Core.Vulkan.Shader;
import ext.handle_wrapper;
import Core.Vulkan.Preinstall;
import std;

export namespace Core::Vulkan{
	struct PipelineTemplate{
		VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

		ShaderChain shaderChain{};

		std::vector<VkDynamicState> dynamicStates{};
		VkPipelineDynamicStateCreateInfo dynamicStateInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		VkPipelineViewportStateCreateInfo viewportState{};
		VkPipelineDepthStencilStateCreateInfo depthStencilState{};

		std::uint32_t dynamicViewportSize{};
		std::vector<VkViewport> staticViewports{};
		std::vector<VkRect2D> staticScissors{};

		[[nodiscard]] std::uint32_t viewportSize() const{
			return dynamicViewportSize > 0 ? dynamicViewportSize : staticViewports.size();
		}

		[[nodiscard]] PipelineTemplate(){
			useDefaultFixedStages();
		}

		PipelineTemplate& useDefaultFixedStages(){
			viewportState = Default::DynamicViewportState<1>;

			info.pInputAssemblyState = &Default::InputAssembly;
			info.pRasterizationState = &Default::Rasterizer;
			info.pMultisampleState = &Default::Multisampling;
			info.pColorBlendState = &Default::ColorBlending<std::array{Blending::ScaledAlphaBlend}>;

			return *this;
		}

		PipelineTemplate& setColorBlend(const VkPipelineColorBlendStateCreateInfo* info){
			this->info.pColorBlendState = info;
			return *this;
		}

		PipelineTemplate& setDepthStencilState(const VkPipelineDepthStencilStateCreateInfo& info){
			depthStencilState = info;
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

		PipelineTemplate& setStaticViewportAndScissor(const std::uint32_t w, const std::uint32_t h){
			return setStaticViewport(std::array{VkViewport{
				.x = 0,
				.y = 0,
				.width = static_cast<float>(w),
				.height = static_cast<float>(h),
				.minDepth = 0.f,
				.maxDepth = 1.f
			}}).setStaticScissors({{}, {w, h}});
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

			info.stageCount = shaderChain.size();
			info.pStages = shaderChain.data();

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

		void apply(VkPipelineLayout layout, VkRenderPass renderPass = nullptr, const std::uint32_t subpassIndex = 0){
			info.layout = layout;
			info.renderPass = renderPass;
			info.subpass = subpassIndex;

			info.stageCount = shaderChain.size();
			info.pStages = shaderChain.data();

			info.pVertexInputState = &vertexInputInfo;

			info.pDynamicState = &dynamicStateInfo;

			info.pViewportState = &viewportState;
			info.pDepthStencilState = &depthStencilState;
		}


		[[nodiscard]] auto create(VkDevice device,
			VkPipelineLayout layout, VkRenderPass renderPass, std::uint32_t subpassIndex = 0){
			apply(layout, renderPass, subpassIndex);

			VkPipeline pipeline{};
			auto rst = vkCreateGraphicsPipelines(device, nullptr, 1, &info, nullptr, &pipeline);
			return std::make_pair(pipeline, rst);
		}
	};

	struct DynamicPipelineTemplate : PipelineTemplate{
		std::vector<VkFormat> usedColorAttachmentFormats{};

		VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.pNext = nullptr,
			.viewMask = 0,
			.depthAttachmentFormat = VK_FORMAT_D24_UNORM_S8_UINT,
			.stencilAttachmentFormat = VK_FORMAT_UNDEFINED
		};

		void pushColorAttachmentFormat(const VkFormat format){
			usedColorAttachmentFormats.push_back(format);
		}

		void pushColorAttachmentFormat(const VkFormat format, std::size_t count){
			for (const auto & repeat : std::ranges::views::repeat(format, count)){
				usedColorAttachmentFormats.push_back(repeat);
			}
			//TODO uses append range
			// ...(std::ranges::views::repeat(format, count));
		}

		void applyDynamicRendering(){
			pipelineRenderingCreateInfo.colorAttachmentCount = usedColorAttachmentFormats.size();
			pipelineRenderingCreateInfo.pColorAttachmentFormats = usedColorAttachmentFormats.data();

			info.pNext = &pipelineRenderingCreateInfo;
		}
	};

	struct Pipeline : ext::wrapper<VkPipeline>{
		ext::dependency<VkDevice> device{};

		[[nodiscard]] Pipeline() = default;

		[[nodiscard]] Pipeline(VkDevice device, VkPipeline pipeline) : wrapper{pipeline}, device{device}{}

		[[nodiscard]] Pipeline(
			VkDevice device,
			PipelineTemplate& pipelineTemplate,
			VkPipelineLayout layout, VkRenderPass renderPass, std::uint32_t subpassIndex = 0) : device{device}{
			auto [p, rst] = pipelineTemplate.create(device, layout, renderPass, subpassIndex);

			if(rst){
				throw std::runtime_error("Failed to create pipeline");
			}

			handle = p;
		}

		[[nodiscard]] Pipeline(
			VkDevice device,
			VkPipelineLayout layout, const VkPipelineCreateFlags flags, const VkPipelineShaderStageCreateInfo& stageInfo) : device{device}{


			VkComputePipelineCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
				.pNext = nullptr,
				.flags = flags,
				.stage = stageInfo,
				.layout = layout,
				.basePipelineHandle = nullptr,
				.basePipelineIndex = 0
			};

			const auto rst = vkCreateComputePipelines(device, nullptr, 1, &createInfo, nullptr, &handle);

			if(rst){
				throw std::runtime_error("Failed to create pipeline");
			}
		}

		~Pipeline(){
			if(device)vkDestroyPipeline(device, handle, nullptr);
		}

		Pipeline(const Pipeline& other) = delete;

		Pipeline(Pipeline&& other) noexcept = default;

		Pipeline& operator=(const Pipeline& other) = delete;

		Pipeline& operator=(Pipeline&& other) noexcept = default;
	};
}

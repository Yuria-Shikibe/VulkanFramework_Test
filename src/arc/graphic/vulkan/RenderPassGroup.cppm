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

import Core.Vulkan.CommandPool;
import Core.Vulkan.RenderPass;

import std;

export namespace Core::Vulkan{
	struct PipelineGroup{
		// VkSubpassContents contents;

		PipelineLayout layout{};
		GraphicPipeline pipeline{};

		DescriptorSetLayout descriptorSetLayout{};
		DescriptorSet descriptorSet{};

		std::vector<UniformBuffer> uniformBuffers{};

	};

	struct RenderPassGroup{
		Dependency<Context*> context{};

		RenderPass renderPass{};
		FrameBuffer framebuffer{};


		std::vector<Attachment> attachments{};

		std::vector<PipelineGroup> subPassSockets{};
		std::vector<VkPipeline> pipelines{};

		DescriptorSetPool descriptorPool{};

		//init
		//set renderpass params
		//set framebuffer params

		void init(Context& context){
			this->context = &context;

			renderPass = RenderPass{context.device};
		}

		void createRenderPass(){
			renderPass.createRenderPass();

			pipelines.resize(renderPass.subpassSize());
		}
	};
}

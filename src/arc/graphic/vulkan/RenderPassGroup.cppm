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

import std;

export namespace Core::Vulkan{
	struct SubPassSocket{
		// VkSubpassContents contents;

		PipelineLayout layout{};
		GraphicPipeline pipeline{};

		DescriptorSetLayout descriptorSetLayout{};
		DescriptorSet descriptorSet{};

		std::vector<UniformBuffer> uniformBuffers{};

	};

	struct RenderPassGroup{
		Dependency<Context*> context{};

		VkRenderPass renderPass{};

		FrameBuffer framebuffer{};
		std::vector<Attachment> attachments{};

		DescriptorSetPool descriptorPool{};

		std::vector<SubPassSocket> subPassSockets{};
	};
}

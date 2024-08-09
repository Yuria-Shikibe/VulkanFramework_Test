module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.RenderPass;

import Core.Vulkan.Dependency;
import std;

export namespace Core::Vulkan{
	namespace Default{

	}

	struct RenderPass : public Wrapper<VkRenderPass>{
		Dependency<VkDevice> device{};

		[[nodiscard]] RenderPass() = default;

		void createRenderPass(VkFormat format){
			VkAttachmentDescription colorAttachment{};

			colorAttachment.format = format;

			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

			//No stencil
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;


			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference colorAttachmentRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

			VkSubpassDescription subpass{
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.inputAttachmentCount = 0,
				.pInputAttachments = nullptr,

				.colorAttachmentCount = 1,
				.pColorAttachments = &colorAttachmentRef,

				.pResolveAttachments = nullptr,
				.pDepthStencilAttachment = nullptr,

				.preserveAttachmentCount = 0,
				.pPreserveAttachments = nullptr
			};

			VkSubpassDependency dependency{};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;

			dependency.dstSubpass = 0;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &colorAttachment;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 1;
			renderPassInfo.pDependencies = &dependency;

			if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &handler) != VK_SUCCESS){
				throw std::runtime_error("failed to create render pass!");
			}
		}

		~RenderPass(){
			if(device)vkDestroyRenderPass(device, handler, nullptr);
		}

		RenderPass(const RenderPass& other) = delete;

		RenderPass(RenderPass&& other) noexcept = default;

		RenderPass& operator=(const RenderPass& other) = delete;

		RenderPass& operator=(RenderPass&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroyRenderPass(device, handler, nullptr);

			Wrapper<VkRenderPass>::operator =(std::move(other));
			device = std::move(other.device);
			return *this;
		}
	};
}
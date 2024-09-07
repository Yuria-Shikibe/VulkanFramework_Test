module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.DynamicRendering;
import std;

export namespace Core::Vulkan{
	struct DynamicRendering{
		std::vector<VkRenderingAttachmentInfo> colorAttachmentsInfo{};
		VkRenderingAttachmentInfo depthInfo{};
		VkRenderingAttachmentInfo stencilInfo{};
		
		VkRenderingInfo info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderArea = {},
			.layerCount = 1,
			.viewMask = 0,
		};

		void setDepthAttachment(VkImageView view, const VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, const VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, const VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE){
			depthInfo = VkRenderingAttachmentInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.pNext = nullptr,
				.imageView = view,
				.imageLayout = layout,
				.resolveMode = VK_RESOLVE_MODE_NONE,
				.resolveImageView = nullptr,
				.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.loadOp = loadOp,
				.storeOp = storeOp,
				.clearValue = {
					.depthStencil = {1.f},
				}
			};
			info.pDepthAttachment = &depthInfo;
		}


		void clearColorAttachments(){
			colorAttachmentsInfo.clear();
		}

		void pushColorAttachment(VkImageView view, const VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, const VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, const VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE){
			colorAttachmentsInfo.push_back(VkRenderingAttachmentInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.pNext = nullptr,
				.imageView = view,
				.imageLayout = layout,
				.resolveMode = VK_RESOLVE_MODE_NONE,
				.resolveImageView = nullptr,
				.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.loadOp = loadOp,
				.storeOp = storeOp,
				.clearValue = {}
			});
		}

		void beginRendering(VkCommandBuffer commandBuffer, const VkRect2D& area){
			info.colorAttachmentCount = colorAttachmentsInfo.size();
			info.pColorAttachments = colorAttachmentsInfo.data();
			info.renderArea = area;
			vkCmdBeginRendering(commandBuffer, &info);
		}
	};

}

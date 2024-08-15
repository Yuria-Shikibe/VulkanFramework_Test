module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Attachment;

import Core.Vulkan.Image;
import Core.Vulkan.CombinedImage;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Buffer.ExclusiveBuffer;

import Geom.Vector2D;
import std;

export namespace Core::Vulkan{
	struct Attachment : CombinedImage{
		using CombinedImage::CombinedImage;

		VkFormat format{};
		VkImageUsageFlags usages{};

		void resize(const Geom::USize2 size, VkCommandBuffer commandBuffer){
			create(size, commandBuffer, usages, format);
		}

		VkDescriptorImageInfo getDescriptorInfo(VkSampler sampler = nullptr, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const{
			return {
				.sampler = sampler,
				.imageView = defaultView,
				.imageLayout = layout
			};
		}

		void create(
			const Geom::USize2 size,
			VkCommandBuffer commandBuffer,
			VkImageUsageFlags usages = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VkFormat format = VK_FORMAT_R8G8B8A8_UNORM
			){

			if(!device || !physicalDevice){
				throw std::runtime_error("Device or PhysicalDevice is null");
			}

			//TODO 3d attachment support
			layers = 1;
			this->size = size;
			this->usages = usages;
			this->format = format;

			image = Image{physicalDevice, device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = format,
				.extent = { size.x, size.y, 1 },
				.mipLevels = 1,
				.arrayLayers = 1, //TODO
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = usages,
			}};

			image.transitionImageLayout(
				commandBuffer,
				format,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);

			defaultView = ImageView{device, image, format};
		}

		Attachment(const Attachment& other) = delete;

		Attachment(Attachment&& other) noexcept = default;

		Attachment& operator=(const Attachment& other) = delete;

		Attachment& operator=(Attachment&& other) noexcept = default;

		void cmdClear(VkCommandBuffer commandBuffer, VkClearColorValue clearColor, VkImageLayout newLayout, VkAccessFlags srcAccessFlags = 0) {
			VkImageSubresourceRange imageSubresourceRange{
				VK_IMAGE_ASPECT_COLOR_BIT,
				0, 1, 0, layers };

			VkImageMemoryBarrier imageMemoryBarrier{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				nullptr,
				srcAccessFlags,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				image,
				imageSubresourceRange
			};

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

			vkCmdClearColorImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &imageSubresourceRange);

			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = 0;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageMemoryBarrier.newLayout = newLayout;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
				0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
		}
	};
}
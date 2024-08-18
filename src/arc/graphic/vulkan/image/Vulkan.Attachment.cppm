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
		VkFormat format{};
		VkImageUsageFlags usages{};
		VkImageLayout bestLayout{};

		std::uint32_t index{};
		//TODO support scale for over/low sampler usage
		float scale{1.f};

		//TODO miplevels support??

		void setScale(const float scale){
			this->scale = scale;
		}

		void resize(Geom::USize2 size, VkCommandBuffer commandBuffer){
			size.scl(scale);
			if(size == this->size) return;

			this->size = size;

			image = Image{
				physicalDevice, device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = format,
					.extent = {size.x, size.y, 1},
					.mipLevels = 1,
					.arrayLayers = 1, //TODO
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.tiling = VK_IMAGE_TILING_OPTIMAL,
					.usage = usages,
				}
			};

			VkImageAspectFlags aspects{};

			if(usages & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT){
				aspects |= VK_IMAGE_ASPECT_COLOR_BIT;
			}

			if(usages & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT){
				aspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
				aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}

			image.transitionImageLayout(
				commandBuffer,
				format,
				VK_IMAGE_LAYOUT_UNDEFINED, bestLayout, 1, aspects);

			defaultView = ImageView{device, image, format, {
				.aspectMask = aspects,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = layers
			}};
		}

		//TODO merge two below to one template function
		void cmdClearColor(VkCommandBuffer commandBuffer,
			VkClearColorValue clearColor,
			VkAccessFlags srcAccessFlags,
			VkImageAspectFlags aspect
		) const{
			VkImageSubresourceRange imageSubresourceRange{
				aspect,
				0, 1, 0, layers
			};

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
			imageMemoryBarrier.newLayout = bestLayout;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
				0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
		}

		void cmdClearDepthStencil(VkCommandBuffer commandBuffer,
			VkClearDepthStencilValue clearValue,
			VkAccessFlags srcAccessFlags,
			VkImageAspectFlags aspect
		) const{
			VkImageSubresourceRange imageSubresourceRange{
				aspect,
				0, 1, 0, layers
			};

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


			vkCmdClearDepthStencilImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &imageSubresourceRange);

			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = 0;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageMemoryBarrier.newLayout = bestLayout;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
				0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
		}

		using CombinedImage::CombinedImage;

		void create(
					const Geom::USize2 size,
					VkCommandBuffer commandBuffer,
					VkImageUsageFlags usages,
					VkFormat format,
					VkImageLayout imageLayout
				){

			if(!device || !physicalDevice){
				throw std::runtime_error("Device or PhysicalDevice is null");
			}

			this->layers = 1;
			this->usages = usages;
			this->format = format;
			this->bestLayout = imageLayout;

			resize(size, commandBuffer);
		}

		VkDescriptorImageInfo getDescriptorInfo(VkSampler sampler = nullptr, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const{
			return {
				.sampler = sampler,
				.imageView = defaultView,
				.imageLayout = layout
			};
		}
	};

	/**
	 * @brief Slice Usage
	 */
	struct ColorAttachment : Attachment{
		using Attachment::Attachment;

		void create(
			const Geom::USize2 size,
			VkCommandBuffer commandBuffer,
			VkImageUsageFlags usages = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VkFormat format = VK_FORMAT_R8G8B8A8_UNORM
		){
			Attachment::create(size, commandBuffer, usages, format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		}

		void resize(const Geom::USize2 size, VkCommandBuffer commandBuffer){
			Attachment::resize(size, commandBuffer);
		}

		void cmdClear(VkCommandBuffer commandBuffer, VkClearColorValue clearColor, VkAccessFlags srcAccessFlags = 0) {
			Attachment::cmdClearColor(commandBuffer, clearColor, srcAccessFlags, VK_IMAGE_ASPECT_COLOR_BIT);
		}
	};


	//TODO as functions instead of class?

	/**
	 * @brief Slice Usage
	 */
	struct DepthStencilAttachment : Attachment{
		using Attachment::Attachment;

		void create(
			const Geom::USize2 size,
			VkCommandBuffer commandBuffer,
			VkImageUsageFlags usages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VkFormat format = VK_FORMAT_D24_UNORM_S8_UINT
		){
			Attachment::create(size, commandBuffer, usages, format, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		}

		void resize(const Geom::USize2 size, VkCommandBuffer commandBuffer){
			Attachment::resize(size, commandBuffer);
		}

		void cmdClear(VkCommandBuffer commandBuffer, VkClearColorValue clearColor, VkAccessFlags srcAccessFlags = 0) const{
			Attachment::cmdClearColor(commandBuffer, clearColor, srcAccessFlags, VK_IMAGE_ASPECT_COLOR_BIT);
		}
	};
}
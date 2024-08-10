module;

#include <vulkan/vulkan.h>
#include <vulkan/utility/vk_format_utils.h>

export module Core.Vulkan.Image;

import Graphic.Pixmap;
import OS.File;

import Core.Vulkan.Memory;
import Core.Vulkan.Buffer.ExclusiveBuffer;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Dependency;
import std;

export namespace Core::Vulkan{
	void createImage(VkPhysicalDevice physicalDevice, VkDevice device,
	                 uint32_t width, uint32_t height, std::uint32_t mipLevels,
	                 VkFormat format, VkImageTiling tiling,
	                 VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
	                 VkImage& image, VkDeviceMemory& imageMemory
	){
		VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if(vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS){
			throw std::runtime_error("failed to create image!");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, physicalDevice, properties);

		if(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS){
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(device, image, imageMemory, 0);
	}

	void transitionImageLayout(
		VkCommandPool commandPool, VkDevice device, VkQueue queue,
		VkImage image, VkFormat format,
		VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels){
		TransientCommand commandBuffer(device, commandPool, queue);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL){
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		} else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout ==
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL){
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		} else{
			throw std::invalid_argument("unsupported layout transition!");
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}

	void copyBufferToImage(
		VkCommandPool commandPool, VkDevice device, VkQueue queue,
		VkBuffer buffer, VkImage image,
		uint32_t width, uint32_t height){
		TransientCommand commandBuffer(device, commandPool, queue);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = {0, 0, 0};
		region.imageExtent = {width, height, 1};

		vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	void generateMipmaps(
		VkCommandPool commandPool, VkDevice device, VkQueue queue,
		VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels){
		// VkFormatProperties formatProperties;
		// vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);
		//
		// if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		// 	throw std::runtime_error("texture image format does not support linear blitting!");
		// }


		TransientCommand commandBuffer(device, commandPool, queue);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for(uint32_t i = 1; i < mipLevels; i++){
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
			                     nullptr,
			                     0, nullptr, 1, &barrier);

			VkImageBlit blit = {};
			blit.srcOffsets[0] = {0, 0, 0};
			blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;

			blit.dstOffsets[0] = {0, 0, 0};
			blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
			               image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			               image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			               1, &blit, VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			                     0, 0, nullptr, 0, nullptr, 1, &barrier);

			if(mipWidth > 1) mipWidth /= 2;
			if(mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
		                     nullptr, 0, nullptr, 1, &barrier);
	}


	void createTextureImage(
		VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkDevice device, VkQueue queue,
		VkImage& textureImage, VkDeviceMemory& textureImageMemory, std::uint32_t& mipLevels
	){
		Graphic::Pixmap pixmap{OS::File{R"(D:\projects\vulkan_framework\properties\texture\src.png)"}};

		Vulkan::StagingBuffer buffer(physicalDevice, device, pixmap.size());

		void* data = buffer.memory.map();
		std::memcpy(data, pixmap.data(), pixmap.size());
		buffer.memory.unmap();

		mipLevels =
			static_cast<std::uint32_t>(std::floor(std::log2(std::max(pixmap.getWidth(), pixmap.getHeight())))) + 1;


		createImage(physicalDevice, device,
		            pixmap.getWidth(), pixmap.getHeight(), mipLevels,
		            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
		            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		            textureImage, textureImageMemory);


		transitionImageLayout(commandPool, device, queue,
		                      textureImage,
		                      VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		                      mipLevels);
		copyBufferToImage(commandPool, device, queue, buffer, textureImage, pixmap.getWidth(), pixmap.getHeight());
		//
		//
		// transitionImageLayout(commandPool, device, queue,
		//                       textureImage,
		//                       VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels);

		generateMipmaps(commandPool, device, queue, textureImage, VK_FORMAT_R8G8B8A8_UNORM, pixmap.getWidth(),
		                pixmap.getHeight(), mipLevels);
	}

	VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, std::uint32_t mipLevels){
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		if(!image || vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS){
			throw std::runtime_error("failed to create texture image view!");
		}

		return imageView;
	}

	class Image : public Wrapper<VkImage>{
		DeviceMemory memory{};
		Dependency<VkDevice> device{};

	public:
		Image() = default;

		~Image(){
			if(device)vkDestroyImage(device, handle, nullptr);
		}

		Image(const Image& other) = delete;

		Image(Image&& other) noexcept = default;

		Image& operator=(const Image& other) = delete;

		Image& operator=(Image&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroyImage(device, handle, nullptr);
			Wrapper<VkImage>::operator =(std::move(other));
			device = std::move(other.device);
			return *this;
		}

		Image(VkPhysicalDevice physicalDevice, VkDevice device,
			VkMemoryPropertyFlags properties,
			const VkImageCreateInfo& imageInfo) : memory{device, properties}{


			if(vkCreateImage(device, &imageInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("failed to create image!");
			}

			memory.allocate(physicalDevice, handle);

			vkBindImageMemory(device, handle, memory, 0);
		}

		Image(VkPhysicalDevice physicalDevice, VkDevice device, VkMemoryPropertyFlags properties,
		      uint32_t width, uint32_t height, std::uint32_t mipLevels,
		      VkFormat format, VkImageTiling tiling,
		      VkImageUsageFlags usage
		) : Image{
				physicalDevice, device, properties, {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = format,
					.extent = {width, height, 1},
					.mipLevels = mipLevels,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.tiling = tiling,
					.usage = usage,
					.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
					.queueFamilyIndexCount = 0,
					.pQueueFamilyIndices = nullptr,
					.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
				}
			}{}

	};

	class ImageView : Wrapper<VkImageView>{
		Dependency<VkDevice> device{};
	public:
		ImageView() = default;

		ImageView(VkDevice device, const VkImageViewCreateInfo& createInfo) : device{device}{
			if(vkCreateImageView(device, &createInfo, nullptr, &handle)){
				throw std::runtime_error("Failed to create image view!");
			}
		}

		ImageView(VkDevice device,
			VkImage image, VkImageViewType viewType, VkFormat format, const VkImageSubresourceRange& subresourceRange, VkImageViewCreateFlags flags = 0) :
			ImageView(device, {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = flags,
				.image = image,
				.viewType = viewType,
				.format = format,
				.components = {},
				.subresourceRange = subresourceRange
			}) {
		}

		~ImageView(){
			if(device)vkDestroyImageView(device, handle, nullptr);
		}

		ImageView(const ImageView& other) = delete;

		ImageView(ImageView&& other) noexcept = default;

		ImageView& operator=(const ImageView& other) = delete;

		ImageView& operator=(ImageView&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroyImageView(device, handle, nullptr);

			Wrapper<VkImageView>::operator =(std::move(other));
			device = std::move(other.device);
			return *this;
		}
	};
}

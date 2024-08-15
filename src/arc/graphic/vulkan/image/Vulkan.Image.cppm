module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Image;

import Core.Vulkan.Memory;
import Core.Vulkan.Buffer.ExclusiveBuffer;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Dependency;
import std;


export namespace Core::Vulkan{
	namespace Util{
		struct ImageFormatTransferCond{
			struct Key{
				VkImageLayout oldLayout{};
				VkImageLayout newLayout{};

				constexpr friend bool operator==(const Key& lhs, const Key& rhs) noexcept{
					return lhs.oldLayout == rhs.oldLayout
						&& lhs.newLayout == rhs.newLayout;
				}
			};

			struct Value{
				VkAccessFlags srcAccessMask{};
				VkAccessFlags dstAccessMask{};
				VkPipelineStageFlags sourceStage{};
				VkPipelineStageFlags destinationStage{};
			};

			struct KeyHasher{
				constexpr std::size_t operator()(const Key& key) const noexcept{
					return std::bit_cast<std::size_t>(key);
				}
			};

			std::unordered_map<Key, Value, KeyHasher> supportTransform{};

			[[nodiscard]] explicit ImageFormatTransferCond(
				const std::initializer_list<decltype(supportTransform)::value_type> supportTransform)
				: supportTransform{supportTransform}{}

			[[nodiscard]] Value find(const VkImageLayout oldLayout, const VkImageLayout newLayout) const{
				if(const auto itr = supportTransform.find(Key{oldLayout, newLayout}); itr != supportTransform.end()){
					return itr->second;
				}else{
					return {
						VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
						VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
						VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
					};
				}
			}
		};

		const ImageFormatTransferCond imageFormatTransferCond{{
					{VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
					{
						0, VK_ACCESS_TRANSFER_WRITE_BIT,
						VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT
					}
				},
				// {
				// 	{VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
				// 	{
				// 		0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				// 		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
				// 	}
				// },
				{
					{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
					{
						VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
					}
				},
			};


		void transitionImageLayout(
			VkCommandBuffer commandBuffer,
			VkImage image, VkFormat format,
			VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels){
			VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
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

			const auto [srcAccessMask, dstAccessMask, sourceStage, destinationStage] =
				Util::imageFormatTransferCond.find(oldLayout, newLayout);

			barrier.srcAccessMask = srcAccessMask;
			barrier.dstAccessMask = dstAccessMask;

			vkCmdPipelineBarrier(
				commandBuffer,
				sourceStage, destinationStage,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}


		std::uint32_t getMipLevel(std::uint32_t w, std::uint32_t h){
			auto logRst = std::log2(std::max(w, h));
			static constexpr double ThresHold = 2;
			return static_cast<std::uint32_t>(std::floor(std::max(0., logRst - ThresHold))) + 1;
		}

		void generateMipmaps(
			VkCommandBuffer commandBuffer,
			VkImage image, VkFormat imageFormat, std::uint32_t texWidth, std::uint32_t texHeight, std::uint32_t mipLevels){
			VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
			constexpr std::uint32_t layerCount = 1;


			barrier.image = image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = layerCount;
			barrier.subresourceRange.levelCount = 1;

			std::int32_t mipWidth = static_cast<std::int32_t>(texWidth);
			std::int32_t mipHeight = static_cast<std::int32_t>(texHeight);

			for(std::uint32_t i = 1; i < mipLevels; i++){
				barrier.subresourceRange.baseMipLevel = i - 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

				vkCmdPipelineBarrier(
					commandBuffer,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &barrier);

				VkImageBlit blit{};
				blit.srcOffsets[0] = {0, 0, 0};
				blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.mipLevel = i - 1;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = layerCount;

				blit.dstOffsets[0] = {0, 0, 0};
				blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.mipLevel = i;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = layerCount;

				vkCmdBlitImage(commandBuffer,
				               image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				               image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				               1, &blit, VK_FILTER_LINEAR);

				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				vkCmdPipelineBarrier(
					commandBuffer,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &barrier);

				if(mipWidth > 1) mipWidth /= 2;
				if(mipHeight > 1) mipHeight /= 2;
			}

			barrier.subresourceRange.baseMipLevel = mipLevels - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(
				commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);
		}
	}


	class Image : public Wrapper<VkImage>{
		DeviceMemory memory{};
		Dependency<VkDevice> device{};

	public:
		Image() = default;

		~Image(){
			if(device) vkDestroyImage(device, handle, nullptr);
		}

		Image(const Image& other) = delete;

		Image(Image&& other) noexcept = default;

		Image& operator=(const Image& other) = delete;

		Image& operator=(Image&& other) noexcept{
			if(this == &other) return *this;
			if(device) vkDestroyImage(device, handle, nullptr);
			Wrapper<VkImage>::operator =(std::move(other));
			memory = std::move(other.memory);
			device = std::move(other.device);
			return *this;
		}

		Image(VkPhysicalDevice physicalDevice, VkDevice device,
		      VkMemoryPropertyFlags properties,
		      const VkImageCreateInfo& imageInfo) : memory{device, properties}, device{device}{
			if(vkCreateImage(device, &imageInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("failed to create image!");
			}

			memory.acquireLimit(physicalDevice);

			if(memory.allocate(physicalDevice, handle)){
				throw std::runtime_error("failed to allocate image!");
			}

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

		void loadBuffer(
			VkCommandBuffer commandBuffer, VkBuffer src,
			VkExtent3D extent, VkOffset3D offset = {}) const{
			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;

			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;

			region.imageOffset = offset;
			region.imageExtent = extent;

			vkCmdCopyBufferToImage(commandBuffer, src, handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		void loadImage(
			VkCommandBuffer commandBuffer, VkImage src,
			VkExtent3D extent, VkOffset3D offset = {}) const = delete;

		void transitionImageLayout(
			VkCommandBuffer commandBuffer, VkFormat format,
			VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels = 1) const{
			Util::transitionImageLayout(commandBuffer, handle, format, oldLayout, newLayout, mipLevels);
		}
	};

	class ImageView : public Wrapper<VkImageView>{
		Dependency<VkDevice> device{};

	public:
		ImageView() = default;

		ImageView(VkDevice device, const VkImageViewCreateInfo& createInfo) : device{device}{
			if(vkCreateImageView(device, &createInfo, nullptr, &handle)){
				throw std::runtime_error("Failed to create image view!");
			}
		}

		ImageView(VkDevice device,
		          VkImage image, VkFormat format, const VkImageSubresourceRange& subresourceRange,
		          VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, VkImageViewCreateFlags flags = 0) :
			ImageView(device, {
				          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				          .pNext = nullptr,
				          .flags = flags,
				          .image = image,
				          .viewType = viewType,
				          .format = format,
				          .components = {},
				          .subresourceRange = subresourceRange
			          }){}

		ImageView(VkDevice device,
		          VkImage image, VkFormat format, const std::uint32_t mipLevels = 1,
		          VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
		          VkImageViewCreateFlags flags = 0) :
			ImageView(device, image, format, {
				          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				          .baseMipLevel = 0,
				          .levelCount = mipLevels,
				          .baseArrayLayer = 0,
				          .layerCount = 1
			          }, viewType, flags){}

		~ImageView(){
			if(device) vkDestroyImageView(device, handle, nullptr);
		}

		ImageView(const ImageView& other) = delete;

		ImageView(ImageView&& other) noexcept = default;

		ImageView& operator=(const ImageView& other) = delete;

		ImageView& operator=(ImageView&& other) noexcept{
			if(this == &other) return *this;
			if(device) vkDestroyImageView(device, handle, nullptr);

			Wrapper<VkImageView>::operator =(std::move(other));
			device = std::move(other.device);
			return *this;
		}
	};


}

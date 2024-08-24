module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Image;

import Core.Vulkan.Memory;
import Core.Vulkan.Buffer.ExclusiveBuffer;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Dependency;
import Core.Vulkan.Concepts;
import Core.Vulkan.Params;

import std;
import Geom.Vector2D;
import Geom.Rect_Orthogonal;

export namespace Core::Vulkan{
	namespace Util{
		struct ImageFormatTransferCond{
			struct Key{
				VkImageLayout oldLayout{};
				VkImageLayout newLayout{};

				constexpr friend bool operator==(const Key& lhs, const Key& rhs) noexcept = default;
			};

			struct Value{
				VkAccessFlags srcAccessMask{};
				VkAccessFlags dstAccessMask{};
				VkPipelineStageFlags srcStage{};
				VkPipelineStageFlags dstStage{};
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

        const ImageFormatTransferCond imageFormatTransferCond{
                {{VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
                 {
                     0,
                     VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT
                 }},

                {{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                 {
                     VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                 }},

                {{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                 {
                     VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT,
                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                 }},

                {{VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
                 {
                     0,
                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                 }},

                {{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
                 {
                     VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_ACCESS_TRANSFER_READ_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT
                 }},

                {{VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
                 {
                     VK_ACCESS_TRANSFER_READ_BIT,
                     VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT
                 }},
            };

	    struct TransitionInfo {
	        // VkFormat format;
	        // VkImageAspectFlags aspect;
	        VkImageLayout oldLayout;
	        VkImageLayout newLayout;
	        VkAccessFlags srcAccessMask;
	        VkAccessFlags dstAccessMask;
	        VkPipelineStageFlags srcStageMask;
            VkPipelineStageFlags dstStageMask;
	    };

	    void transitionImageLayout(
            VkCommandBuffer commandBuffer,
            VkImage image, const TransitionInfo &info,
            const VkImageSubresourceRange &subresourceRange
        ) {
	        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	        barrier.oldLayout = info.oldLayout;
	        barrier.newLayout = info.newLayout;
	        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	        barrier.image = image;
	        barrier.subresourceRange = subresourceRange;

	        barrier.srcAccessMask = info.srcAccessMask;
	        barrier.dstAccessMask = info.dstAccessMask;

	        vkCmdPipelineBarrier(
                commandBuffer,
                info.srcStageMask, info.dstStageMask,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
	    }

        void transitionImageLayout(
            VkCommandBuffer commandBuffer,
            VkImage image, const TransitionInfo &info,
            VkImageAspectFlags aspect,
            const std::uint32_t mipLevels = 1, const std::uint32_t layers = 1
            ) {
            transitionImageLayout(commandBuffer, image, info, {
                .aspectMask = aspect,
                .baseMipLevel = 0,
                .levelCount = mipLevels,
                .baseArrayLayer = 0,
                .layerCount = layers
            });
        }

		void transitionImageLayout(
			VkCommandBuffer commandBuffer,
			VkImage image, VkFormat format,
			VkImageLayout oldLayout, VkImageLayout newLayout,
			const std::uint32_t mipLevels, const std::uint32_t layers,
			VkImageAspectFlags aspect){
			const auto [srcAccessMask, dstAccessMask, sourceStage, destinationStage] =
				Util::imageFormatTransferCond.find(oldLayout, newLayout);

            transitionImageLayout(commandBuffer, image, {
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcAccessMask = srcAccessMask,
                .dstAccessMask = dstAccessMask,
                .srcStageMask = sourceStage,
                .dstStageMask = destinationStage
            }, aspect, mipLevels, layers);
		}


		std::uint32_t getMipLevel(const std::uint32_t w, const std::uint32_t h){
			auto logRst = std::log2(std::max(w, h));
			static constexpr double Threshold = 2;
			return static_cast<std::uint32_t>(std::floor(std::max(0., logRst - Threshold))) + 1;
		}

	    void blit(
	        VkCommandBuffer commandBuffer,
            VkImage src, VkImage dst,
            VkOffset3D srcBegin, VkOffset3D srcEnd, VkImageSubresourceLayers srcRange,
            VkOffset3D dstBegin, VkOffset3D dstEnd, VkImageSubresourceLayers dstRange,
            VkFilter filter = VK_FILTER_LINEAR,
            VkImageLayout srcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VkImageLayout dstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        ) {
            const VkImageBlit blit{
                .srcSubresource = srcRange,
                .srcOffsets = {srcBegin, srcEnd},
                .dstSubresource = dstRange,
                .dstOffsets = {dstBegin, dstEnd}
            };
            
            vkCmdBlitImage(commandBuffer,
                               src, srcLayout,
                               dst, dstLayout,
                               1, &blit, filter);
        }
	    
	    void blit(
	        VkCommandBuffer commandBuffer,
            VkImage src, VkImage dst,
            const Geom::Rect_Orthogonal<std::int32_t> srcRegion, VkImageSubresourceLayers srcRange,
            const Geom::Rect_Orthogonal<std::int32_t> dstRegion, VkImageSubresourceLayers dstRange,
            VkFilter filter = VK_FILTER_LINEAR,
            VkImageLayout srcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VkImageLayout dstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        ) {
            blit(commandBuffer,
                src, dst,
                {srcRegion.getSrcX(), srcRegion.getSrcY()},
                {srcRegion.getEndX(), srcRegion.getEndY(), 1},
                srcRange,

                {dstRegion.getSrcX(), dstRegion.getSrcY()},
                {dstRegion.getEndX(), dstRegion.getEndY(), 1},
                dstRange,

                filter, srcLayout, dstLayout);
        }


        void blit(
            VkCommandBuffer commandBuffer,
            VkImage src, VkImage dst,
            const Geom::Rect_Orthogonal<std::int32_t> srcRegion,
            const Geom::Rect_Orthogonal<std::int32_t> dstRegion,
            const VkFilter filter = VK_FILTER_LINEAR,
            const std::uint32_t srcMipmapLevel = 0, const std::uint32_t dstMipmapLevel = 0,
            const std::uint32_t layers = 1,
            const VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            const VkImageLayout srcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            const VkImageLayout dstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            ) {
            blit(commandBuffer,
                 src, dst,
                 srcRegion, {aspect, srcMipmapLevel, 0, layers},

                 dstRegion, {aspect, dstMipmapLevel, 0, layers},
                 filter, srcLayout, dstLayout);
        }

		void generateMipmaps(
			VkCommandBuffer commandBuffer,
			VkImage image,
			VkFormat imageFormat,
			const std::uint32_t texWidth, const std::uint32_t texHeight,
			const std::uint32_t mipLevels,
			const std::uint32_t layerCount = 1
		){
			VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
			barrier.image = image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = layerCount;
			barrier.subresourceRange.levelCount = 1;

            Geom::OrthoRectInt srcRegion{static_cast<int>(texWidth), static_cast<int>(texHeight)};

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

                const Geom::OrthoRectInt dstRegion{srcRegion.getWidth() > 1 ? srcRegion.getWidth() / 2 : 1, srcRegion.getHeight() > 1 ? srcRegion.getHeight() / 2 : 1};

                blit(
                    commandBuffer, image, image,
                    srcRegion,
                    dstRegion,
                    VK_FILTER_LINEAR,
                    i - 1, i
                    );

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


			    srcRegion = dstRegion;
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
              const std::uint32_t width, const std::uint32_t height, const std::uint32_t mipLevels,
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
			VkExtent3D extent, VkOffset3D offset = {}, std::uint32_t layers = 1) const{
			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;

			region.imageSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = layers
			};

			region.imageOffset = offset;
			region.imageExtent = extent;

			vkCmdCopyBufferToImage(commandBuffer, src, handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		void loadImage(
			VkCommandBuffer commandBuffer, VkImage src,
			VkExtent3D extent, VkOffset3D offset = {}) const = delete;

		void transitionImageLayout(
			VkCommandBuffer commandBuffer, VkFormat format,
			VkImageLayout oldLayout, VkImageLayout newLayout,
			const std::uint32_t mipLevels = 1, const std::uint32_t layers = 1, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT) const{
			Util::transitionImageLayout(commandBuffer, handle, format, oldLayout, newLayout, mipLevels, layers, aspectFlags);
		}

        void transitionImageLayout(
            VkCommandBuffer commandBuffer,
            const Util::TransitionInfo &info,
            const std::uint32_t mipLevels = 1, const std::uint32_t layers = 1) const {
            Util::transitionImageLayout(commandBuffer, handle, info, mipLevels, layers);
        }


		template <ContigiousRange<VkImageBlit> Rng = std::vector<VkImageBlit>>
		void blit(VkCommandBuffer commandBuffer, VkImage src, VkFilter filter, Rng&& rng){
			::vkCmdBlitImage(commandBuffer,
						   src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						   std::ranges::size(rng), std::ranges::data(rng), filter);
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

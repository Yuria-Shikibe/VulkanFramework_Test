module;

#include <vulkan/vulkan.h>
#include <vulkan/utility/vk_format_utils.h>

export module Core.Vulkan.Image;


import Core.Vulkan.Memory;
import Core.Vulkan.Buffer.ExclusiveBuffer;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Dependency;
import std;

import Graphic.Pixmap;
import OS.File;
import Geom.Vector2D;

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
				}

				throw std::invalid_argument("unsupported layout transition!");
			}
		};

		const ImageFormatTransferCond imageFormatTransferCond{
				{
					{VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
					{
						0, VK_ACCESS_TRANSFER_WRITE_BIT,
						VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT
					}
				},
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
			static constexpr double ThresHold = 4;
			return static_cast<std::uint32_t>(std::floor(std::max(0., logRst - ThresHold))) + 1;
		}

		void generateMipmaps(
			VkCommandBuffer commandBuffer,
			VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels){
			VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
			constexpr std::uint32_t layerCount = 1;


			barrier.image = image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = layerCount;
			barrier.subresourceRange.levelCount = 1;

			int32_t mipWidth = texWidth;
			int32_t mipHeight = texHeight;


			for(uint32_t i = 1; i < mipLevels; i++){
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

	class Texture{
		Image image{};
		ImageView defaultView{};

		Dependency<VkPhysicalDevice> physicalDevice{};
		Dependency<VkDevice> device{};

		std::uint32_t mipLevels{};
		std::uint32_t layers{};

		Geom::USize2 size{};

	public:
		[[nodiscard]] Texture() = default;

		[[nodiscard]] Texture(VkPhysicalDevice physicalDevice, VkDevice device)
			: physicalDevice{physicalDevice},
			  device{device}{}

		[[nodiscard]] Texture(VkPhysicalDevice physicalDevice, VkDevice device, const Graphic::Pixmap& pixmap,
		                      TransientCommand&& commandBuffer)
			: Texture{physicalDevice, device}{
			loadPixmap(pixmap, std::move(commandBuffer));
			setImageView();
		}

		[[nodiscard]] Texture(VkPhysicalDevice physicalDevice, VkDevice device, const OS::File& file,
		                      TransientCommand&& commandBuffer)
			: Texture{physicalDevice, device}{
			const Graphic::Pixmap pixmap{file};

			loadPixmap(pixmap, std::move(commandBuffer));
			setImageView();
		}

		//TODO 3D image support

		[[nodiscard]] explicit Texture(std::vector<Graphic::Pixmap> layers) = delete;

		void setImageView(const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM){
			if(!image) return;
			defaultView = ImageView(device, image, format, mipLevels);;
		}

		void loadPixmap(const OS::File& file, TransientCommand&& commandBuffer){
			loadPixmap(Graphic::Pixmap(file), std::move(commandBuffer));
		}

		void loadPixmap(const Graphic::Pixmap& pixmap, TransientCommand&& commandBuffer){
			if(!device || !physicalDevice){
				throw std::runtime_error("Device or PhysicalDevice is null");
			}


			layers = 1;
			size = pixmap.size2D();

			const StagingBuffer buffer(physicalDevice, device, pixmap.size());

			buffer.memory.loadData(pixmap.data(), pixmap.size());

			mipLevels = Util::getMipLevel(pixmap.getWidth(), pixmap.getHeight());

			auto relay = std::move(commandBuffer);

			image = Image(
				physicalDevice, device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				pixmap.getWidth(), pixmap.getHeight(), mipLevels,
				VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			);

			image.transitionImageLayout(
				relay,
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				mipLevels);

			image.loadBuffer(relay, buffer, {pixmap.getWidth(), pixmap.getHeight(), 1});

			Util::generateMipmaps(
				relay,
				image,
				VK_FORMAT_R8G8B8A8_UNORM, pixmap.getWidth(), pixmap.getHeight(), mipLevels);
		}

		[[nodiscard]] const Image& getImage() const noexcept{ return image; }

		[[nodiscard]] const ImageView& getView() const noexcept{ return defaultView; }

		[[nodiscard]] VkDevice getDevice() const noexcept{ return device; }

		[[nodiscard]] VkPhysicalDevice getPhysicalDevice() const noexcept{ return physicalDevice; }

		[[nodiscard]] std::uint32_t getMipLevels() const noexcept{ return mipLevels; }

		[[nodiscard]] std::uint32_t getLayers() const noexcept{ return layers; }

		[[nodiscard]] Geom::USize2 getSize() const noexcept{ return size; }
	};
}

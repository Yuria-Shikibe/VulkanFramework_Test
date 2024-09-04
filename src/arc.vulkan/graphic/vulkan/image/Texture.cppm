module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Texture;

import Core.Vulkan.Image;
import Core.Vulkan.CombinedImage;
import ext.handle_wrapper;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Buffer.ExclusiveBuffer;
import std;

import Graphic.Pixmap;
import Core.File;
import Geom.Vector2D;
import Geom.Rect_Orthogonal;

export namespace Core::Vulkan{
	class Texture : public CombinedImage{
		std::uint32_t mipLevels{};

	public:
		using CombinedImage::CombinedImage;

		[[nodiscard]] Texture(VkPhysicalDevice physicalDevice, VkDevice device, const Graphic::Pixmap& pixmap,
		                      TransientCommand&& commandBuffer)
			: Texture{physicalDevice, device}{
			loadPixmap(std::move(commandBuffer), pixmap);
		}

		[[nodiscard]] Texture(VkPhysicalDevice physicalDevice, VkDevice device, const Core::File& file,
		                      TransientCommand&& commandBuffer)
			: Texture{physicalDevice, device}{
			const Graphic::Pixmap pixmap{file};

			loadPixmap(std::move(commandBuffer), pixmap);
		}

		//TODO 3D image support

		[[nodiscard]] explicit Texture(std::vector<Graphic::Pixmap> layers) = delete;

		void setImageView(const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM){
			if(!image) return;
			defaultView = ImageView(device, {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.image = image,
				.viewType = layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
				.format = format,
				.components = {},
				.subresourceRange = VkImageSubresourceRange{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = mipLevels,
					.baseArrayLayer = 0,
					.layerCount = layers
				}
			});
		}

		void createEmpty(const Geom::USize2 size2, const std::uint32_t layers){
			this->size = size2;
			this->layers = layers;

			setMipmap();

			image = Image(
				physicalDevice, device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = VK_FORMAT_R8G8B8A8_UNORM,
					.extent = {
						size.x, size.y, 1
					},
					.mipLevels = mipLevels,
					.arrayLayers = layers,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.tiling = VK_IMAGE_TILING_OPTIMAL,
					.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
					.queueFamilyIndexCount = 0,
					.pQueueFamilyIndices = nullptr,
					.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
			});

			setImageView();
		}

		Graphic::Pixmap exportToPixmap(TransientCommand&& commandBuffer) const{
			const StagingBuffer stagingBuffer{physicalDevice, device, size.area() * Graphic::Pixmap::Channels,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT
			};

			image.transitionImageLayout(commandBuffer, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			image.exportToBuffer(commandBuffer, stagingBuffer, {
				size.x, size.y, 1
			}, {}, 0u, layers);

			image.transitionImageLayout(commandBuffer, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			commandBuffer = {};

			Graphic::Pixmap pixmap{};
			pixmap.create(size.x, size.y);
			auto* dst = pixmap.data();

			auto* src = stagingBuffer.memory.map();

			std::memcpy(dst, src, pixmap.sizeBytes());

			return pixmap;
		}

		void loadPixmap(TransientCommand&& commandBuffer, const File& file){
			loadPixmap(std::move(commandBuffer), Graphic::Pixmap(file));
		}

		void loadPixmap(TransientCommand&& commandBuffer, const Graphic::Pixmap& pixmap){
			if(!device || !physicalDevice){
				throw std::runtime_error("Device or PhysicalDevice is null");
			}

			layers = 1;
			size = pixmap.size2D();

			const StagingBuffer buffer(physicalDevice, device, pixmap.sizeBytes());
			buffer.memory.loadData(pixmap.data(), pixmap.sizeBytes());

			completeLoad(std::move(commandBuffer), buffer);
		}

		template <std::ranges::sized_range Rng = std::vector<Graphic::Pixmap>>
			requires (std::same_as<std::ranges::range_value_t<Rng>, Graphic::Pixmap>)
		void loadPixmap(TransientCommand&& commandBuffer, Rng&& pixmaps){
			if(!device || !physicalDevice){
				throw std::runtime_error("Device or PhysicalDevice is null");
			}

			layers = std::ranges::size(pixmaps);
			Graphic::Pixmap& pixmap = *std::ranges::begin(pixmaps);
			size = pixmap.size2D();

			const StagingBuffer buffer(physicalDevice, device, pixmap.sizeBytes() * layers);

			auto* p = static_cast<Graphic::Pixmap::DataType*>(buffer.memory.map());
			for(const auto& [index, map] : pixmaps | std::views::enumerate){
				if(map.size2D() != size){
					throw std::runtime_error("Size Mismatch When Loading Image Array");
				}
				std::memcpy(p + index * pixmap.sizeBytes(), map.data(), map.sizeBytes());
			}
			buffer.memory.unmap();

			completeLoad(std::move(commandBuffer), buffer);
		}

		// void loadPixmap(const Graphic::Pixmap& pixmap, TransientCommand&& commandBuffer){}

		[[nodiscard]] std::uint32_t getMipLevels() const noexcept{ return mipLevels; }

		void writeImage(VkCommandBuffer commandBuffer, VkImage src, const Geom::OrthoRectInt srcRegion, const Geom::OrthoRectInt dstRegion) const{
			image.transitionImageLayout(
							commandBuffer,
							VK_FORMAT_R8G8B8A8_UNORM,
							VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							mipLevels, layers,
							VK_IMAGE_ASPECT_COLOR_BIT);

			Util::blit(commandBuffer, src, image, srcRegion, dstRegion, VK_FILTER_LINEAR, 0, 0, layers);

			Util::generateMipmaps(
				commandBuffer,
				image, dstRegion, mipLevels, layers);
		}

		void writeBuffer(VkCommandBuffer commandBuffer, VkBuffer src, const Geom::OrthoRectInt dstRegion) const{
			image.transitionImageLayout(
							commandBuffer,
							VK_FORMAT_R8G8B8A8_UNORM,
							VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							mipLevels, layers,
							VK_IMAGE_ASPECT_COLOR_BIT);

			image.loadBuffer(commandBuffer, src,
			                 {
				                 static_cast<std::uint32_t>(dstRegion.getWidth()),
				                 static_cast<std::uint32_t>(dstRegion.getHeight()), 1
			                 },
			                 {dstRegion.getSrcX(), dstRegion.getSrcY()}, layers);

			Util::generateMipmaps(
				commandBuffer,
				image, dstRegion, mipLevels, layers);
		}

	protected:
		void setMipmap(){
			mipLevels = std::min(Util::getMipLevel(size.x, size.y), 10u);
		}

	public:
		void completeLoad(TransientCommand&& commandBuffer, VkBuffer dataSource){
			setMipmap();

			image = Image(
				physicalDevice, device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = VK_FORMAT_R8G8B8A8_UNORM,
					.extent = {
						size.x, size.y, 1
					},
					.mipLevels = mipLevels,
					.arrayLayers = layers,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.tiling = VK_IMAGE_TILING_OPTIMAL,
					.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
					.queueFamilyIndexCount = 0,
					.pQueueFamilyIndices = nullptr,
					.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
			});

			image.transitionImageLayout(
				commandBuffer,
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				mipLevels, layers,
				VK_IMAGE_ASPECT_COLOR_BIT);

			image.loadBuffer(commandBuffer, dataSource, {size.x, size.y, 1}, {}, layers);

			auto [w, h] = size.as<int>();

			Util::generateMipmaps(
				commandBuffer,
				image, Geom::OrthoRectInt{w, h}, mipLevels, layers);

			commandBuffer = {};
			setImageView();
		}

		void cmdClearColor(VkCommandBuffer commandBuffer,
		                   VkClearColorValue clearValue,
		                   VkAccessFlags srcAccessFlags = VK_ACCESS_SHADER_READ_BIT,
		                   VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT
		) const{
			CombinedImage::cmdClearColor(commandBuffer, clearValue, srcAccessFlags, aspect,
			                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		void cmdClearDepthStencil(VkCommandBuffer commandBuffer,
		                          VkClearDepthStencilValue clearValue,
		                          VkAccessFlags srcAccessFlags,
		                          VkImageAspectFlags aspect
		) const{
			CombinedImage::cmdClearDepthStencil(commandBuffer, clearValue, srcAccessFlags, aspect, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	};
}

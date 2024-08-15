module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Texture;

import Core.Vulkan.Image;
import Core.Vulkan.CombinedImage;
import Core.Vulkan.Dependency;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Buffer.ExclusiveBuffer;
import std;

import Graphic.Pixmap;
import OS.File;
import Geom.Vector2D;

export namespace Core::Vulkan{
	class Texture : public CombinedImage{
		std::uint32_t mipLevels{};

	public:
		using CombinedImage::CombinedImage;

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

		[[nodiscard]] std::uint32_t getMipLevels() const noexcept{ return mipLevels; }
	};
}

module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.CombinedImage;

import Core.Vulkan.Image;
import Core.Vulkan.Dependency;

import std;
import Geom.Vector2D;

export namespace Core::Vulkan{
	struct CombinedImage{
	protected:
		Image image{};
		ImageView defaultView{};

		Dependency<VkPhysicalDevice> physicalDevice{};
		Dependency<VkDevice> device{};

		std::uint32_t layers{};

		Geom::USize2 size{};

	public:
		[[nodiscard]] CombinedImage() = default;

		[[nodiscard]] CombinedImage(VkPhysicalDevice physicalDevice, VkDevice device)
			: physicalDevice{physicalDevice},
			  device{device}{}

		[[nodiscard]] const Image& getImage() const noexcept{ return image; }

		[[nodiscard]] const ImageView& getView() const noexcept{ return defaultView; }

		[[nodiscard]] VkDevice getDevice() const noexcept{ return device; }

		[[nodiscard]] VkPhysicalDevice getPhysicalDevice() const noexcept{ return physicalDevice; }

		[[nodiscard]] std::uint32_t getLayers() const noexcept{ return layers; }

		[[nodiscard]] Geom::USize2 getSize() const noexcept{ return size; }
	};
}

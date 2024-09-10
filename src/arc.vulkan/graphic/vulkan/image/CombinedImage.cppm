module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.CombinedImage;

import Core.Vulkan.Image;
import ext.handle_wrapper;

import std;
import Geom.Vector2D;
import ext.Concepts;

export namespace Core::Vulkan{
	struct CombinedImage{
	protected:
		Image image{};
		ImageView defaultView{};

		ext::dependency<VkPhysicalDevice> physicalDevice{};
		ext::dependency<VkDevice> device{};

		std::uint32_t layers{1};

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

		template <typename Fn>
		void cmdClear(Fn fn, VkCommandBuffer commandBuffer,
		              std::add_lvalue_reference_t<std::remove_pointer_t<std::tuple_element_t<
			              3, typename ext::FunctionTraits<std::remove_pointer_t<Fn>>::ArgsTuple>>> clearValue,
		              VkAccessFlags srcAccessFlags,
		              VkImageAspectFlags aspect,
		              VkImageLayout layout
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

			fn(commandBuffer, image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &imageSubresourceRange);

			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = 0;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageMemoryBarrier.newLayout = layout;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			                     0,
			                     0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
		}

		void cmdClearColor(VkCommandBuffer commandBuffer,
		                   VkClearColorValue clearValue,
		                   VkAccessFlags srcAccessFlags,
		                   VkImageAspectFlags aspect,
		                   VkImageLayout layout
		) const{
			cmdClear(vkCmdClearColorImage, commandBuffer, clearValue, srcAccessFlags, aspect, layout);
		}

		void cmdClearDepthStencil(VkCommandBuffer commandBuffer,
		                          VkClearDepthStencilValue clearValue,
		                          VkAccessFlags srcAccessFlags,
		                          VkImageAspectFlags aspect,
		                          VkImageLayout layout
		) const{
			cmdClear(vkCmdClearDepthStencilImage, commandBuffer, clearValue, srcAccessFlags, aspect, layout);
		}
	};
}

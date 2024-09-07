module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Util;

export import Core.Vulkan.Util.Invoker;
import std;


namespace Core::Vulkan::Util{
	export std::vector<VkExtensionProperties> getValidExtensions(){
		auto [extensions, rst] = enumerate(vkEnumerateInstanceExtensionProperties, nullptr);

		return extensions;
	}

	export
	template <std::ranges::range Rng>
		requires (std::convertible_to<std::ranges::range_value_t<Rng>, VkImageView>)
	[[nodiscard]] std::vector<VkDescriptorImageInfo> getDescriptorInfoRange_ShaderRead(VkSampler handle, const Rng& imageViews) noexcept{
		std::vector<VkDescriptorImageInfo> rst{};

		for (const auto& view : imageViews){
			rst.emplace_back(handle, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		return rst;
	}

	export
	template <typename T>
		requires requires(T& t){
			t.srcAccessMask;
			t.dstAccessMask;
			t.srcStageMask;
			t.dstStageMask;

			std::swap(t.srcAccessMask, t.dstAccessMask);
			std::swap(t.srcStageMask, t.dstStageMask);
		}
	void swapStage(T& t){
		std::swap(t.srcAccessMask, t.dstAccessMask);
		std::swap(t.srcStageMask, t.dstStageMask);
	}

	export
	template <>

	void swapStage(VkImageMemoryBarrier2& t){
		std::swap(t.srcAccessMask, t.dstAccessMask);
		std::swap(t.srcStageMask, t.dstStageMask);
		std::swap(t.srcQueueFamilyIndex, t.srcQueueFamilyIndex);
		std::swap(t.oldLayout, t.newLayout);
	}



	export
	template <std::ranges::range Range, std::ranges::range ValidRange, typename Proj = std::identity>
		requires requires(
			std::unordered_set<std::string>& set,
			std::indirect_result_t<Proj, std::ranges::const_iterator_t<ValidRange>> value
		){
			requires std::convertible_to<std::ranges::range_value_t<Range>, std::string>;
			// set.erase(value);
		}
	[[nodiscard]] bool checkSupport(const Range& required, const ValidRange& valids, Proj proj = {}){
		std::unordered_set<std::string> requiredExtensions(std::ranges::begin(required), std::ranges::end(required));

		for(const auto& extension : valids){
			requiredExtensions.erase(std::invoke(proj, extension));
		}

		return requiredExtensions.empty();
	}
}

module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Sampler;

import std;
import ext.handle_wrapper;
import Core.Vulkan.Comp;

export namespace Core::Vulkan{
	namespace SamplerInfo{
		constexpr Util::Component Filter_Linear{
				VkSamplerCreateInfo{
					.magFilter = VK_FILTER_LINEAR,
					.minFilter = VK_FILTER_LINEAR
				},
				&VkSamplerCreateInfo::magFilter, &VkSamplerCreateInfo::minFilter
			};

	    constexpr Util::Component Filter_PIXEL_Linear{
				VkSamplerCreateInfo{
					.magFilter = VK_FILTER_NEAREST,
					.minFilter = VK_FILTER_LINEAR
				},
				&VkSamplerCreateInfo::magFilter, &VkSamplerCreateInfo::minFilter
			};

		constexpr Util::Component Filter_Nearest{
				VkSamplerCreateInfo{
					.magFilter = VK_FILTER_NEAREST,
					.minFilter = VK_FILTER_NEAREST
				},
				&VkSamplerCreateInfo::magFilter, &VkSamplerCreateInfo::minFilter
			};

		constexpr Util::Component AddressMode_Repeat{
				VkSamplerCreateInfo{
					.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
					.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
					.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				},
				&VkSamplerCreateInfo::addressModeU, &VkSamplerCreateInfo::addressModeV, &VkSamplerCreateInfo::addressModeW
			};

		constexpr Util::Component AddressMode_Clamp{
				VkSamplerCreateInfo{
					.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
					.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
					.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				},
				&VkSamplerCreateInfo::addressModeU, &VkSamplerCreateInfo::addressModeV, &VkSamplerCreateInfo::addressModeW
			};

		constexpr Util::Component LOD_Max{
				VkSamplerCreateInfo{
					.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
					.mipLodBias = 0,
					.minLod = 0,
					.maxLod = VK_LOD_CLAMP_NONE,
				},
				&VkSamplerCreateInfo::mipmapMode, &VkSamplerCreateInfo::minLod,
				&VkSamplerCreateInfo::maxLod, &VkSamplerCreateInfo::mipLodBias
			};

		constexpr Util::Component LOD_Max_Nearest{
				VkSamplerCreateInfo{
					.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
					.mipLodBias = 0,
					.minLod = 0,
					.maxLod = VK_LOD_CLAMP_NONE,
				},
				&VkSamplerCreateInfo::mipmapMode, &VkSamplerCreateInfo::minLod,
				&VkSamplerCreateInfo::maxLod, &VkSamplerCreateInfo::mipLodBias
			};

		constexpr Util::Component LOD_None{
				VkSamplerCreateInfo{
					.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
					.mipLodBias = 0,
					.minLod = 0,
					.maxLod = 0,
				},
				&VkSamplerCreateInfo::mipmapMode, &VkSamplerCreateInfo::minLod,
				&VkSamplerCreateInfo::maxLod, &VkSamplerCreateInfo::mipLodBias
			};


		//designators must appear in member declaration order of class
		template <VkCompareOp op = VK_COMPARE_OP_NEVER>
		constexpr Util::Component CompareOp{
				VkSamplerCreateInfo{
					.compareEnable = op,
					.compareOp = op,
				},
				&VkSamplerCreateInfo::compareEnable, &VkSamplerCreateInfo::compareOp
			};

		template <std::uint32_t anisotropy = 4>
		constexpr Util::Component Anisotropy{
				VkSamplerCreateInfo{
					.anisotropyEnable = static_cast<bool>(anisotropy),
					.maxAnisotropy = anisotropy,
				},
				&VkSamplerCreateInfo::anisotropyEnable, &VkSamplerCreateInfo::maxAnisotropy
			};

		constexpr Util::Component Default{
				VkSamplerCreateInfo{
					.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
					.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
					.unnormalizedCoordinates = false
				},
				&VkSamplerCreateInfo::sType,
				&VkSamplerCreateInfo::borderColor,
				&VkSamplerCreateInfo::unnormalizedCoordinates
			};

		constexpr VkSamplerCreateInfo TextureSampler = SamplerInfo::Default
			| SamplerInfo::Filter_Linear
			| SamplerInfo::AddressMode_Clamp
			| SamplerInfo::LOD_Max
			| SamplerInfo::CompareOp<VK_COMPARE_OP_NEVER>
			| SamplerInfo::Anisotropy<4>;
	}

	namespace Util{
		[[nodiscard]] constexpr VkDescriptorImageInfo
			getDescriptorInfo_ShaderRead(VkSampler handle, VkImageView imageView) noexcept{
			return {
				.sampler = handle,
				.imageView = imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
		}

		template <typename ...T>
			requires (std::convertible_to<T, VkImageView> && ...)
		[[nodiscard]] constexpr auto getDescriptorInfo_ShaderRead(VkSampler handle, const T&... imageViews) noexcept{
			std::array<VkDescriptorImageInfo, sizeof...(T)> rst{};

			[&]<std::size_t ...I>(std::index_sequence<I...>){
				((
					rst[I].imageView = imageViews,
					rst[I].sampler = handle,
					rst[I].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				), ...);
			}(std::index_sequence_for<T...>{});

			return rst;
		}

		template <std::ranges::range Rng>
			requires (std::convertible_to<std::ranges::range_value_t<Rng>, VkImageView>)
		[[nodiscard]] std::vector<VkDescriptorImageInfo> getDescriptorInfoRange_ShaderRead(VkSampler handle, const Rng& imageViews) noexcept{
			std::vector<VkDescriptorImageInfo> rst{};

			for (const auto& view : imageViews){
				rst.emplace_back(handle, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}

			return rst;
		}

		VkDescriptorImageInfo getDescriptorInfo(VkSampler handle){
			return {
				.sampler = handle,
			};
		}
	}
	
	class Sampler : public ext::wrapper<VkSampler>{
		ext::dependency<VkDevice> device{};

	public:
		using wrapper::wrapper;
		[[nodiscard]] constexpr Sampler() = default;

		[[nodiscard]] explicit Sampler(VkDevice device, VkSampler textureSampler = nullptr)
			: wrapper{textureSampler}, device{device}{}

		[[nodiscard]] Sampler(VkDevice device, const VkSamplerCreateInfo& samplerInfo)
			: device{device}{
			if(vkCreateSampler(device, &samplerInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("failed to create texture sampler!");
			}
		}

		~Sampler(){
			if(device)vkDestroySampler(device, handle, nullptr);
		}

		Sampler(const Sampler& other) = delete;

		Sampler(Sampler&& other) noexcept = default;

		Sampler& operator=(const Sampler& other) = delete;

		Sampler& operator=(Sampler&& other) noexcept = default;

		[[nodiscard]] constexpr VkDescriptorImageInfo
			getDescriptorInfo_ShaderRead(VkImageView imageView, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const noexcept{
			return {
					.sampler = handle,
					.imageView = imageView,
					.imageLayout = layout
				};
		}

		template <typename ...T>
			requires (std::convertible_to<T, VkImageView> && ...)
		[[nodiscard]] constexpr auto getDescriptorInfo_ShaderRead(const T&... imageViews) const noexcept{
			return Util::getDescriptorInfo_ShaderRead(handle, imageViews...);
		}

		template <std::ranges::sized_range Rng>
			requires (std::convertible_to<std::ranges::range_value_t<Rng>, VkImageView>)
		[[nodiscard]] constexpr auto getDescriptorInfo_ShaderRead(const Rng& imageViews) const noexcept{
			return Util::getDescriptorInfo_ShaderRead(handle, imageViews);

		}

		VkDescriptorImageInfo getDescriptorInfo() const{
			return Util::getDescriptorInfo(handle);
		}
	};
}
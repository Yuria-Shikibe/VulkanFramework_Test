module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Sampler;

import std;
import Core.Vulkan.Dependency;
import Core.Vulkan.Comp;

export namespace Core::Vulkan{
	namespace Samplers{
		constexpr Util::Component<VkSamplerCreateInfo, 0, &VkSamplerCreateInfo::magFilter, &VkSamplerCreateInfo::minFilter> Filter_Linear{{
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR
			}};

		constexpr Util::Component<VkSamplerCreateInfo, 1, &VkSamplerCreateInfo::magFilter, &VkSamplerCreateInfo::minFilter> Filter_Nearest{{
				.magFilter = VK_FILTER_NEAREST,
				.minFilter = VK_FILTER_NEAREST
			}};

		constexpr Util::Component<VkSamplerCreateInfo, 0, &VkSamplerCreateInfo::addressModeU, &VkSamplerCreateInfo::addressModeV, &VkSamplerCreateInfo::addressModeW> AddressMode_Repeat{{
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			}};

		constexpr Util::Component<
			VkSamplerCreateInfo, 0,
			&VkSamplerCreateInfo::mipmapMode, &VkSamplerCreateInfo::minLod,
			&VkSamplerCreateInfo::maxLod, &VkSamplerCreateInfo::mipLodBias> LOD_Max{{
					.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
					.mipLodBias = 0,
					.minLod = 0,
					.maxLod = VK_LOD_CLAMP_NONE,
				}};


		//designators must appear in member declaration order of class
		template <VkCompareOp op = VK_COMPARE_OP_NEVER>
		constexpr Util::Component<
			VkSamplerCreateInfo, op,
			&VkSamplerCreateInfo::compareEnable, &VkSamplerCreateInfo::compareOp> CompareOp{{
					.compareEnable = op,
					.compareOp = op,
				}};

		template <std::uint32_t anisotropy = 4>
		constexpr Util::Component<VkSamplerCreateInfo, anisotropy,
		&VkSamplerCreateInfo::anisotropyEnable, &VkSamplerCreateInfo::maxAnisotropy> Anisotropy{VkSamplerCreateInfo{
				.anisotropyEnable = static_cast<bool>(anisotropy),
				.maxAnisotropy = anisotropy,
			}};

		constexpr Util::Component<VkSamplerCreateInfo, 0,
		&VkSamplerCreateInfo::sType,
		&VkSamplerCreateInfo::borderColor,
		&VkSamplerCreateInfo::unnormalizedCoordinates
		> Default{{
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
				.unnormalizedCoordinates = false
			}};

		constexpr VkSamplerCreateInfo TextureSampler = VkSamplerCreateInfo{}
			| Samplers::Default
			| Samplers::Filter_Linear
			| Samplers::AddressMode_Repeat
			| Samplers::LOD_Max
			| Samplers::CompareOp<VK_COMPARE_OP_NEVER>
			| Samplers::Anisotropy<4>;
	}
	
	class Sampler : public Wrapper<VkSampler>{
		DeviceDependency device{};

	public:
		[[nodiscard]] constexpr Sampler() = default;

		[[nodiscard]] explicit Sampler(VkDevice device, VkSampler textureSampler = nullptr)
			: Wrapper{textureSampler}, device{device}{}

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

		Sampler& operator=(Sampler&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroySampler(device, handle, nullptr);

			Wrapper<VkSampler>::operator =(std::move(other));
			device = std::move(other.device);
			return *this;
		}

		[[nodiscard]] constexpr VkDescriptorImageInfo
			getDescriptorInfo_ShaderRead(VkImageView imageView) const noexcept{
			return {
					.sampler = handle,
					.imageView = imageView,
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				};
		}

		template <typename ...T>
			requires (std::convertible_to<T, VkImageView> && ...)
		[[nodiscard]] constexpr auto getDescriptorInfo_ShaderRead(const T&... imageViews) const noexcept{
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
	};
}
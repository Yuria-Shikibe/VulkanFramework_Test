module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Sampler;

import std;
import Core.Vulkan.Dependency;

export namespace Core::Vulkan{
	
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
	};

	[[nodiscard]] Sampler createTextureSampler(VkDevice device, const std::uint32_t mipLevels){
		VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;

		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

		samplerInfo.anisotropyEnable = true;
		samplerInfo.maxAnisotropy = 16;

		samplerInfo.unnormalizedCoordinates = false;

		samplerInfo.compareEnable = false;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod = 0; // Optional
		samplerInfo.maxLod = static_cast<float>(mipLevels);
		samplerInfo.mipLodBias = 0; // Optional

		Sampler textureSampler{device, samplerInfo};

		return textureSampler;
	}
}
module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.DescriptorSet;

import ext.handle_wrapper;
import Core.Vulkan.Concepts;

export import Core.Vulkan.DescriptorLayout;

import std;

export namespace Core::Vulkan{
	class DescriptorSet : public ext::wrapper<VkDescriptorSet>{

	};

	static_assert(std::is_standard_layout_v<DescriptorSet>);
	static_assert(sizeof(DescriptorSet) == sizeof(VkDescriptorSet));

	class DescriptorSetPool : public ext::wrapper<VkDescriptorPool>{
	public:
		ext::dependency<VkDevice> device{};
		ext::dependency<VkDescriptorSetLayout> layout{};

		[[nodiscard]] DescriptorSetPool() = default;

		~DescriptorSetPool(){
			if(device)vkDestroyDescriptorPool(device, handle, nullptr);
		}

		DescriptorSetPool(const DescriptorSetPool& other) = delete;

		DescriptorSetPool(DescriptorSetPool&& other) noexcept = default;

		DescriptorSetPool& operator=(const DescriptorSetPool& other) = delete;

		DescriptorSetPool& operator=(DescriptorSetPool&& other) noexcept = default;

		DescriptorSetPool(VkDevice device, const DescriptorLayout& layout, std::uint32_t size) : device{device}, layout{layout}{
			auto bindings = layout.builder.exportBindings();

			std::vector<VkDescriptorPoolSize> poolSizes(bindings.size());

			for (const auto& [i, binding] : bindings | std::views::enumerate){
				poolSizes[i] = {.type = binding.descriptorType, .descriptorCount = size};
			}

			VkDescriptorPoolCreateFlags flags{};

			if(layout.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT){
				flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
			}

			VkDescriptorPoolCreateInfo poolInfo{
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
					.pNext = nullptr,
					.flags = flags,
					.maxSets = size,
					.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
					.pPoolSizes = poolSizes.data()
			};

			if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &handle) != VK_SUCCESS) {
				throw std::runtime_error("failed to create descriptor pool!");
			}
		}

		[[nodiscard]] DescriptorSet obtain() const{
			DescriptorSet descriptors{};

			VkDescriptorSetAllocateInfo allocInfo{
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
					.pNext = nullptr,
					.descriptorPool = handle,
					.descriptorSetCount = 1,
					.pSetLayouts = layout.asData()
				};

			if (vkAllocateDescriptorSets(device, &allocInfo, descriptors.as_data()) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate descriptor sets!");
			}

			return descriptors;
		}

		[[nodiscard]] std::vector<DescriptorSet> obtain(const std::size_t size) const{
			std::vector<DescriptorSet> descriptors(size);
			std::vector layouts(size, layout.handle);

			VkDescriptorSetAllocateInfo allocInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.pNext = nullptr,
				.descriptorPool = handle,
				.descriptorSetCount = static_cast<std::uint32_t>(size),
				.pSetLayouts = layouts.data()
			};

			if (auto rst = vkAllocateDescriptorSets(device, &allocInfo, reinterpret_cast<VkDescriptorSet*>(descriptors.data())); rst != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate descriptor sets!");
			}

			return descriptors;
		}
	};

	class DescriptorSetUpdator{
		static constexpr VkWriteDescriptorSet DefaultSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = nullptr,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM,
			.pImageInfo = nullptr,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr
		};

		ext::dependency<VkDevice> device{};
		DescriptorSet& descriptorSets;
		std::vector<VkWriteDescriptorSet> descriptorWrites{};

	public:

		[[nodiscard]] explicit DescriptorSetUpdator(VkDevice device, DescriptorSet& descriptorSets)
			: device{device}, descriptorSets{descriptorSets}{
		}

		//TODO manually spec binding index

		void push(VkDescriptorBufferInfo& uniformBufferInfo){
			const auto index = descriptorWrites.size();

			auto& current = descriptorWrites.emplace_back(DefaultSet);
			current.dstSet = descriptorSets;
			current.dstBinding = index;
			current.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			current.pBufferInfo = &uniformBufferInfo;
		}

		void add(std::uint32_t index, VkDescriptorBufferInfo& uniformBufferInfo){
			auto& current = descriptorWrites.emplace_back(DefaultSet);
			current.dstSet = descriptorSets;
			current.dstBinding = index;
			current.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			current.pBufferInfo = &uniformBufferInfo;
		}

		void pushSampledImage(VkDescriptorImageInfo& imageInfo){
			const auto index = descriptorWrites.size();

			auto& current = descriptorWrites.emplace_back(DefaultSet);
			current.dstSet = descriptorSets;
			current.dstBinding = index;
			current.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			current.pImageInfo = &imageInfo;
		}

		void pushImage(VkDescriptorImageInfo& imageInfo, VkDescriptorType type){
			const auto index = descriptorWrites.size();

			auto& current = descriptorWrites.emplace_back(DefaultSet);
			current.dstSet = descriptorSets;
			current.dstBinding = index;
			current.descriptorType = type;
			current.pImageInfo = &imageInfo;
		}

		void pushAttachment(VkDescriptorImageInfo& imageInfo){
			const auto index = descriptorWrites.size();

			auto& current = descriptorWrites.emplace_back(DefaultSet);
			current.dstSet = descriptorSets;
			current.dstBinding = index;
			current.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			current.pImageInfo = &imageInfo;
		}

		void push(ContigiousRange<VkDescriptorImageInfo> auto& imageInfos){
			const auto index = descriptorWrites.size();

			auto& current = descriptorWrites.emplace_back(DefaultSet);
			current.dstSet = descriptorSets;
			current.dstBinding = index;
			current.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			current.pImageInfo = std::ranges::data(imageInfos);
			current.descriptorCount = std::ranges::size(imageInfos);
		}

		void update() const{
			vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
		}
	};
}


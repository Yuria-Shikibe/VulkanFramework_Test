module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.DescriptorSet;

import Core.Vulkan.Dependency;
import Core.Vulkan.Concepts;

import std;
import ext.RuntimeException;

export namespace Core::Vulkan{
	class DescriptorSet : public Wrapper<VkDescriptorSet>{

	};

	static_assert(std::is_standard_layout_v<DescriptorSet>);
	static_assert(sizeof(DescriptorSet) == sizeof(VkDescriptorSet));

	class DescriptorSetLayoutBuilder{
		struct VkDescriptorSetLayoutBinding_Mapper{
			constexpr bool operator()(const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs) const noexcept{
				return lhs.binding < rhs.binding;
			}
		};

		std::set<VkDescriptorSetLayoutBinding, VkDescriptorSetLayoutBinding_Mapper> bindings{};

	public:
		void push(const VkDescriptorSetLayoutBinding& binding){
			const auto [rst, success] = bindings.insert(binding);

			if(!success){
				throw ext::RuntimeException(std::format("Conflicted Desc Layout At[{}]", binding.binding));
			}
		}

		void push(const std::uint32_t index, const VkDescriptorType type, const VkShaderStageFlags stageFlags, const std::uint32_t count = 1){
			push({
				.binding = index,
				.descriptorType = type,
				.descriptorCount = count,
				.stageFlags = stageFlags,
				.pImmutableSamplers = nullptr
			});
		}

		void push_seq(const VkDescriptorType type, const VkShaderStageFlags stageFlags, const std::uint32_t count = 1){
			push(bindings.size(), type, stageFlags, count);
		}

		[[nodiscard]] std::vector<VkDescriptorSetLayoutBinding> exportBindings() const{
			return {bindings.begin(), bindings.end()};
		}
	};

	class DescriptorSetLayout : public Wrapper<VkDescriptorSetLayout>{
		Dependency<VkDevice> device{};

	public:
		DescriptorSetLayoutBuilder builder{};

		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
		std::vector<VkDescriptorBindingFlags> bindingFlags{};
		VkDescriptorSetLayoutCreateFlags flags{};

		[[nodiscard]] DescriptorSetLayout() = default;

		~DescriptorSetLayout(){
			if(device)vkDestroyDescriptorSetLayout(device, handle, nullptr);
		}

		DescriptorSetLayout(const DescriptorSetLayout& other) = delete;

		DescriptorSetLayout(DescriptorSetLayout&& other) noexcept = default;

		DescriptorSetLayout& operator=(const DescriptorSetLayout& other) = delete;

		DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroyDescriptorSetLayout(device, handle, nullptr);

			Wrapper::operator=(std::move(other));
			builder = std::move(other.builder);
			device = std::move(other.device);
			flags = other.flags;
			return *this;
		}

		DescriptorSetLayout(VkDevice device, std::regular_invocable<DescriptorSetLayout&> auto&& func) : device{device}{
			func(*this);

			create();
		}

		void pushBindingFlag(VkDescriptorBindingFlagBits flags){
			bindingFlags.push_back(flags);
		}

	private:
		void create(){
			const auto bindings = builder.exportBindings();

			bindingFlags.resize(bindings.size());
			bindingFlagsCreateInfo.bindingCount = bindingFlags.size();
			bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

			const VkDescriptorSetLayoutCreateInfo layoutInfo{
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
					.pNext = &bindingFlagsCreateInfo,
					.flags = flags,
					.bindingCount = static_cast<std::uint32_t>(bindings.size()),
					.pBindings = bindings.data()
				};

			if(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create descriptor set layout!");
			}
		}
	};

	class DescriptorSetPool : public Wrapper<VkDescriptorPool>{
	public:
		Dependency<VkDevice> device{};
		Dependency<VkDescriptorSetLayout> layout{};

		[[nodiscard]] DescriptorSetPool() = default;

		~DescriptorSetPool(){
			if(device)vkDestroyDescriptorPool(device, handle, nullptr);
		}

		DescriptorSetPool(const DescriptorSetPool& other) = delete;

		DescriptorSetPool(DescriptorSetPool&& other) noexcept = default;

		DescriptorSetPool& operator=(const DescriptorSetPool& other) = delete;

		DescriptorSetPool& operator=(DescriptorSetPool&& other) noexcept{
			if(this == &other) return *this;
			this->~DescriptorSetPool();
			Wrapper::operator=(std::move(other));
			device = std::move(other.device);
			layout = std::move(other.layout);
			return *this;
		}

		DescriptorSetPool(VkDevice device, const DescriptorSetLayout& layout, std::uint32_t size) : device{device}, layout{layout}{
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

			if (vkAllocateDescriptorSets(device, &allocInfo, descriptors.operator->()) != VK_SUCCESS) {
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

		Dependency<VkDevice> device{};
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


module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.DescriptorSet;

import Core.Vulkan.LogicalDevice.Dependency;
import std;
import ext.RuntimeException;

export namespace Core::Vulkan{

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
			push({.binding = index, .descriptorType = type, .descriptorCount = count, .stageFlags = stageFlags, .pImmutableSamplers = nullptr});
		}

		void push_seq(const VkDescriptorType type, const VkShaderStageFlags stageFlags, const std::uint32_t count = 1){
			push(bindings.size(), type, stageFlags, count);
		}

		[[nodiscard]] std::vector<VkDescriptorSetLayoutBinding> exportBindings() const{
			return {bindings.begin(), bindings.end()};
		}
	};

	class DescriptorSetLayout{
		VkDescriptorSetLayout descriptorSetLayout{};
		DeviceDependency device{};

	public:
		DescriptorSetLayoutBuilder builder{};

		operator VkDescriptorSetLayout() const noexcept{return descriptorSetLayout;}

		const VkDescriptorSetLayout* operator->() const noexcept{return &descriptorSetLayout; }

		[[nodiscard]] const VkDescriptorSetLayout& get() const{ return descriptorSetLayout; }

		[[nodiscard]] DescriptorSetLayout() = default;

		~DescriptorSetLayout(){
			if(device)vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		}

		DescriptorSetLayout(const DescriptorSetLayout& other) = delete;

		DescriptorSetLayout(DescriptorSetLayout&& other) noexcept = default;

		DescriptorSetLayout& operator=(const DescriptorSetLayout& other) = delete;

		DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

			descriptorSetLayout = other.descriptorSetLayout;
			builder = std::move(other.builder);
			device = std::move(other.device);
			return *this;
		}

		DescriptorSetLayout(VkDevice device, std::regular_invocable<DescriptorSetLayout&> auto&& func) : device{device}{
			func(*this);

			create();
		}

	private:
		void create(){
			const auto bindings = builder.exportBindings();

			const VkDescriptorSetLayoutCreateInfo layoutInfo{
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.bindingCount = static_cast<std::uint32_t>(bindings.size()),
					.pBindings = bindings.data()
				};

			if(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS){
				throw std::runtime_error("Failed to create descriptor set layout!");
			}
		}
	};

	class DescriptorSetPool{
		Dependency<VkDevice> device{};
		VkDescriptorPool descriptorPool{};

	public:
		operator VkDescriptorPool() const noexcept{ return descriptorPool; }

		[[nodiscard]] VkDescriptorPool& get() noexcept{ return descriptorPool; }

		[[nodiscard]] DescriptorSetPool() = default;

		~DescriptorSetPool(){
			if(device)vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		}

		DescriptorSetPool(const DescriptorSetPool& other) = delete;

		DescriptorSetPool(DescriptorSetPool&& other) noexcept = default;

		DescriptorSetPool& operator=(const DescriptorSetPool& other) = delete;

		DescriptorSetPool& operator=(DescriptorSetPool&& other) noexcept{
			if(this == &other) return *this;
			this->~DescriptorSetPool();
			device = std::move(other.device);
			descriptorPool = other.descriptorPool;
			return *this;
		}

		DescriptorSetPool(VkDevice device, const DescriptorSetLayoutBuilder& layout, std::uint32_t size) : device{device}{
			auto bindings = layout.exportBindings();

			std::vector<VkDescriptorPoolSize> poolSizes(bindings.size());

			for (const auto& [i, binding] : bindings | std::views::enumerate){
				poolSizes[i] = {.type = binding.descriptorType, .descriptorCount = size};
			}

			VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
			poolInfo.poolSizeCount = poolSizes.size();
			poolInfo.pPoolSizes = poolSizes.data();
			poolInfo.maxSets = size;

			if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
				throw std::runtime_error("failed to create descriptor pool!");
			}
		}
	};
}


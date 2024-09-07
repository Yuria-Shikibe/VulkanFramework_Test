module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.DescriptorLayout;

import ext.handle_wrapper;
import Core.Vulkan.Concepts;

import std;

export namespace Core::Vulkan{
	class DescriptorSetLayoutBuilder{
		struct VkDescriptorSetLayoutBinding_Mapper{
			constexpr bool operator()(const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs) const noexcept{
				return lhs.binding < rhs.binding;
			}
		};

		std::set<VkDescriptorSetLayoutBinding, VkDescriptorSetLayoutBinding_Mapper> bindings{};

	public:
		[[nodiscard]] std::uint32_t size() const noexcept{
			return bindings.size();
		}

		void push(const VkDescriptorSetLayoutBinding& binding){
			const auto [rst, success] = bindings.insert(binding);

			if(!success){
				std::println(std::cerr, "Conflicted Desc Layout At[{}]", binding.binding);
				throw std::runtime_error("Conflicted Layout Binding");
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

	class DescriptorSetLayout : public ext::wrapper<VkDescriptorSetLayout>{
		ext::dependency<VkDevice> device{};

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

		DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept = default;

		DescriptorSetLayout(VkDevice device, std::regular_invocable<DescriptorSetLayout&> auto&& func) : device{device}{
			func(*this);

			create();
		}

		void pushBindingFlag(VkDescriptorBindingFlagBits flags){
			bindingFlags.push_back(flags);
		}

		[[nodiscard]] std::uint32_t size() const noexcept{
			return builder.size();
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
}
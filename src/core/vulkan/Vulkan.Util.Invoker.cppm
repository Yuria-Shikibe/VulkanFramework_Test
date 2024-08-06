module;

#include <vulkan/vulkan.h>
#include <cassert>

export module Core.Vulkan.Util.Invoker;

import std;

namespace Core::Vulkan::Util{
	template <::std::size_t index, typename... Ts>
	struct find;

	template <::std::size_t index, typename T, typename... Ts>
	struct find<index, T, Ts...>{
		using type = typename find<index - 1, Ts...>::type;
	};
	template <typename T, typename... Ts>
	struct find<0, T, Ts...>{
		using type = T;
	};
	template <::std::size_t index>
	struct find<index>{
		using type = ::std::nullptr_t;
	};

	template <typename T>
	struct RstPair{
		std::vector<T> Result{};
		VkResult Code{VK_SUCCESS};
	};

	template <typename... Ts>
	using get_last_t = typename find<sizeof...(Ts) - 1u, Ts...>::type;
	template <::std::size_t drop, typename... Ts>
	using get_last_n_t = typename find<sizeof...(Ts) - 1u - drop, Ts...>::type;
	//
	export
	template <typename T, typename... Prms, typename... Args>
	[[nodiscard]] auto enumerate(T (*fn)(Prms...), Args... args)
		requires::std::invocable<T(*)(Prms...), Args..., ::std::uint32_t*, get_last_t<Prms...>>
	{
		assert(fn/*,"Invoke function cannot be empty."*/);

		if constexpr(!std::is_void_v<T>){

			RstPair<std::remove_pointer_t<get_last_t<Prms...>>> result{};

			::uint32_t nums;
			if((result.Code = fn(args..., &nums, nullptr)) != VK_SUCCESS) return result;
			result.Result.resize(nums);
			result.Code = fn(args..., &nums, result.Result.data());
			return result;
		} else{
			::std::vector<std::remove_pointer_t<get_last_t<Prms...>>> result{};

			::uint32_t nums;
			fn(args..., &nums, nullptr);

			result.resize(nums);
			fn(args..., &nums, result.data());

			return result;
		}
	}

	/*
	export
	template <typename T, typename... Prms, typename... Args>
	[[nodiscard]] auto Invoke(T (*fn)(Prms...), Args... args) noexcept
		requires::std::invocable<T(*)(Prms...), Args..., get_last_t<Prms...>>{
		assert(fn/*, "Invoke function cannot be empty."#1#);
		using result_t = ::std::remove_pointer_t<get_last_t<Prms...>>;
		if constexpr(!::std::is_void_v<T>){
			struct{
				result_t Result{};
				::VkResult Code{VK_SUCCESS};
			} result;

			result.Code = fn(args..., &result.Result);
			return result;
		} else{
			result_t result;
			fn(args..., &result);
			return result;
		}
	}

	//WARNING: Use this method to invoke allocate command buffer, not the generic template.
	export
	auto Invoke(decltype(&::vkAllocateCommandBuffers) fnAllocate, ::VkDevice device,
	            ::VkCommandBufferAllocateInfo const* info){
		struct result_type{
			::std::vector<::VkCommandBuffer> Result;
			::VkResult Code;
		};

		::std::vector<::VkCommandBuffer> buffers{info->commandBufferCount};
		auto code = fnAllocate(device, info, buffers.data());
		return result_type{::std::move(buffers), code};
	}

	//WARNING: Use this method to invoke create pipeline, not the generic template.
	export
	auto Invoke(decltype(&::vkCreateGraphicsPipelines) fnCreate,
	            ::VkDevice device,
	            ::VkPipelineCache pipelineCache,
	            ::std::span<::VkGraphicsPipelineCreateInfo> createInfos,
	            const ::VkAllocationCallbacks* pAllocator){
		struct result_type{
			::std::vector<::VkPipeline> Result;
			::VkResult Code;
		};

		::std::vector<::VkPipeline> pipes{createInfos.size()};
		auto code = fnCreate(device, pipelineCache, static_cast<::uint32_t>(createInfos.size()), createInfos.data(),
		                     pAllocator, pipes.data());

		return result_type{::std::move(pipes), code};
	}

	export
	auto Invoke(decltype(&::vkAllocateDescriptorSets) fn, ::VkDevice device, ::VkDescriptorSetAllocateInfo const* info){
		struct result_t{
			::std::vector<::VkDescriptorSet> Result;
			::VkResult Code;
		};
		::std::vector<::VkDescriptorSet> result{info->descriptorSetCount};
		auto code = fn(device, info, result.data());
		return result_t{::std::move(result), code};
	}*/
}

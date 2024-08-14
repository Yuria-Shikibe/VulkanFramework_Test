module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Comp;

import std;
import ext.MetaProgramming;

namespace Core::Vulkan{
	namespace Util{
		template <typename T, typename Args>
		struct MemPtrOf{
			using value = Args T::*;
		};

		template <typename T, typename Args>
		using mptr = typename MemPtrOf<T, Args>::value;

		export template <typename T, typename... Args>
		struct Component{
			static constexpr std::size_t size = sizeof...(Args);

			T val{};
			std::tuple<mptr<T, Args>...> assignMptrs{};

			[[nodiscard]] constexpr Component() = default;

			[[nodiscard]] constexpr explicit Component(const T& val)
				: val{val}{}

			template <typename... A>
			constexpr [[nodiscard]] explicit Component(const T& v, A... args) : val{v}, assignMptrs{args...}{}

			template <std::size_t Idx>
			constexpr void copy(T& dst, const T& src) const noexcept{
				auto mptr = std::get<Idx>(assignMptrs);
				std::invoke(mptr, dst) = std::invoke(mptr, src);
			}



			template <typename A>
				requires (std::same_as<T, std::decay_t<A>>)
			constexpr friend decltype(auto) operator|=(A&& target, const Component& src){
				[&]<std::size_t... I>(std::index_sequence<I...>){
					(src.template copy<I>(target, src.val), ...);
				}(std::make_index_sequence<size>{});

				return std::forward<A>(target);
			}


			constexpr friend T operator|(const T& target, const Component& src){
				T rst = target;

				[&]<std::size_t... I>(std::index_sequence<I...>){
					(src.template copy<I>(rst, src.val), ...);
				}(std::make_index_sequence<size>{});

				return rst;
			}

			//Support replace mode?
		};


		export template <typename T, typename... Args>
		Component(T, Args...) -> Component<T, typename ext::GetMemberPtrInfo<Args>::ValueType...>;

		export template <typename T>
		auto defStructureType(VkStructureType sType){
			return Component{T{sType}, &T::sType};
		}
	}


	export namespace AttachmentDesc{
		constexpr Util::Component Stencil_DontCare{
				VkAttachmentDescription{
					.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
				},
				&VkAttachmentDescription::stencilLoadOp,
				&VkAttachmentDescription::stencilStoreOp
			};

		constexpr Util::Component Default{
				VkAttachmentDescription{
					.flags = 0,
					.samples = VK_SAMPLE_COUNT_1_BIT
				},
				&VkAttachmentDescription::flags,
				&VkAttachmentDescription::samples
			};
	}

	export namespace Subpass{
		namespace Dependency{
			constexpr Util::Component External{
					VkSubpassDependency{
						.srcSubpass = VK_SUBPASS_EXTERNAL,
						.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
						.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT
					},
					&VkSubpassDependency::srcSubpass,
					&VkSubpassDependency::srcStageMask,
					&VkSubpassDependency::srcAccessMask
				};
		}
	}

}

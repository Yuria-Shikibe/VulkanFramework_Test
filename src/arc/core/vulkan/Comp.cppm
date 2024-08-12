module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Comp;

import std;

export namespace Core::Vulkan::Util{
	template <typename T, std::size_t capture, auto ... Mptrs>
		requires (std::conjunction_v<std::is_member_pointer<decltype(Mptrs)>...>)
	struct Component{
		static constexpr std::size_t size = sizeof...(Mptrs);

		T val{};
		static constexpr std::tuple<decltype(Mptrs)...> assignMptrs{Mptrs...};

		[[nodiscard]] constexpr Component() = default;

		[[nodiscard]] constexpr explicit Component(const T& val)
			: val{val}{}

		template <std::size_t Idx>
		static constexpr void copy(T& dst, const T& src) noexcept{
			auto mptr = std::get<Idx>(assignMptrs);
			std::invoke(mptr, dst) = std::invoke(mptr, src);
		}

		template <typename A>
			requires (std::same_as<T, std::decay_t<A>>)
		constexpr friend decltype(auto) operator|(A&& target, const Component& src){
			[&]<std::size_t... I>(std::index_sequence<I...>){
				(Component::copy<I>(target, src.val), ...);
			}(std::make_index_sequence<size>{});

			return std::forward<A>(target);
		}

		//Support replace mode?
	};


	template <typename T>
	auto defStructureType(VkStructureType sType){
		return Component<T,  &T::sType> {T{sType}};
	}
}

export module Core.Vulkan.Concepts;

import std;

namespace Core::Vulkan{
	export
	template <typename T>
	using EmptyRange = std::initializer_list<T>;

	export
	template <typename Rng, typename Value>
	concept ContigiousRange = requires{
		requires std::ranges::sized_range<Rng>;
		requires std::ranges::contiguous_range<Rng>;
		requires std::same_as<Value, std::ranges::range_value_t<Rng>>;
	};

	export
	template <typename Rng, typename Value>
	concept RangeOf = requires{
		requires std::ranges::range<Rng>;
		requires std::same_as<Value, std::ranges::range_value_t<Rng>>;
	};

	static_assert(ContigiousRange<EmptyRange<int>, int>);
}

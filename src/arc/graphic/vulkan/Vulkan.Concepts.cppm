export module Core.Vulkan.Concepts;

import std;

namespace Core::Vulkan{
	export
	template <typename T = void*>
	[[no_unique_address]] struct EmptyRange{
		struct iterator{
			using iterator_category = std::contiguous_iterator_tag;
			using value_type        = T;
			using difference_type   = std::ptrdiff_t;

			constexpr auto& operator ++() noexcept{return *this;}

			constexpr auto operator ++(int) noexcept{return *this;}

			constexpr auto& operator --() noexcept{return *this;}

			constexpr auto operator --(int) noexcept{return *this;}

			constexpr auto& operator+=(difference_type) noexcept{return *this;}
			constexpr auto& operator-=(difference_type) noexcept{return *this;}
			constexpr auto operator+(difference_type) const noexcept{return *this;}
			constexpr auto operator-(difference_type) const noexcept{return *this;}

			constexpr difference_type operator-(const iterator&) const noexcept{return 0;}

			constexpr auto operator<=>(const iterator&) const noexcept{return std::strong_ordering::equal;}

			constexpr bool operator==(const iterator&) const noexcept{
				return true;
			}

			constexpr T& operator[](difference_type) const{
				throw std::runtime_error("Cannot dereference a empty range");
			}

			constexpr T* operator->() const noexcept{
				return nullptr;
			}

			constexpr T& operator*() const{
				throw std::runtime_error("Cannot dereference a empty range");
			}

			constexpr friend auto operator+(difference_type, const iterator& i) noexcept{return i;}
		};

		[[nodiscard]] friend constexpr std::size_t size() noexcept{return 0;}

		[[nodiscard]] friend constexpr T* data() noexcept{return nullptr;}

		[[nodiscard]] constexpr auto begin() const noexcept{return iterator{};}
		[[nodiscard]] constexpr auto end() const noexcept{return iterator{};}
	};

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

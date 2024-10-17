export module ext.type_map;

import std;
import ext.meta_programming;

export namespace ext{
	template <typename T, typename... Ts>
		requires (std::is_default_constructible_v<T>)
	struct type_map : type_to_index<std::tuple<Ts...>>{
	private:
		using base = type_to_index<std::tuple<Ts...>>;

	public:
		using value_type = T;

		std::array<T, base::arg_size> items{};

		template <std::size_t I>
		[[nodiscard]] constexpr auto& at() const noexcept{
			static_assert(I < base::arg_size, "invalid index");

			return items[I];
		}

		template <std::size_t I>
		[[nodiscard]] constexpr auto& at() noexcept{
			static_assert(I < base::arg_size, "invalid index");

			return items[I];
		}

		template <typename V>
		[[nodiscard]] constexpr auto& at() const noexcept{
			return at<base::template index_of<V>>();
		}

		template <typename V>
		[[nodiscard]] constexpr auto& at() noexcept{
			return at<base::template index_of<V>>();
		}
	};
}

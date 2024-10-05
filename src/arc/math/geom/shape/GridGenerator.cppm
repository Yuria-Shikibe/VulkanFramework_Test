module;

#include "../src/ext/assume.hpp"
#include <cassert>

export module Geom.GridGenerator;

export import Geom.Vector2D;
export import Geom.Rect_Orthogonal;
import std;

export namespace Geom{
	template <std::size_t anchorPointCount = 4>
		requires (anchorPointCount >= 2)
	struct grid_property{
		using size_type = unsigned;
		static constexpr size_type side_size = anchorPointCount - 1;
		static constexpr size_type size = side_size * side_size;

		[[nodiscard]] static constexpr size_type pos_to_index(const size_type x, const size_type y) noexcept{
			return y * side_size + x;
		}

		[[nodiscard]] static constexpr Vector2D<size_type> index_to_pos(const size_type index) noexcept{
			return Vector2D<size_type>(index % side_size, index / side_size);
		}

		static constexpr size_type center_index = pos_to_index(side_size / 2, side_size / 2);

		static constexpr std::array<size_type, (side_size - 2) * 4> edge_indices = []() constexpr {
			constexpr auto edgeSize = side_size - 2;
			constexpr auto sentinel = side_size - 1;
			std::array<size_type, edgeSize * 4> rst{};

			for(const auto [yIdx, y] : std::array<size_type, 2>{0, sentinel} | std::views::enumerate){
				for(size_type x = 1; x < sentinel; ++x){
					rst[(x - 1) + edgeSize * yIdx] = pos_to_index(x, y);
				}
			}

			for(const auto [xIdx, x] : std::array<size_type, 2>{0, sentinel} | std::views::enumerate){
				for(size_type y = 1; y < sentinel; ++y){
					rst[(y - 1) + edgeSize * (xIdx + 2)] = pos_to_index(x, y);
				}
			}
			return rst;
		}();

		static constexpr std::array<size_type, 4> corner_indices = []() constexpr {
			constexpr auto sentinel = side_size - 1;
			std::array<size_type, 4> rst{};

			rst[0] = 0;
			rst[1] = sentinel;
			rst[2] = size - 1 - sentinel;
			rst[3] = size - 1;

			return rst;
		}();
	};

	template <std::size_t anchorPointCount = 4, typename T = float>
		requires (anchorPointCount >= 2 && std::is_arithmetic_v<T>)
	struct grid_generator{
		using property = grid_property<anchorPointCount>;
		using size_type = typename property::size_type;
		static constexpr size_type side_size = property::side_size;
		static constexpr size_type size = property::size;

		using input_type = std::array<Vector2D<T>, anchorPointCount>;
		using result_type = std::array<Rect_Orthogonal<T>, size>;

		[[nodiscard]] static constexpr size_type pos_to_index(const typename Vector2D<size_type>::PassType pos) noexcept{
			return property::pos_to_index(pos.x, pos.y);
		}

		//OPTM static operator
		[[nodiscard]] constexpr result_type operator()(const input_type& diagonalAnchorPoints) const noexcept{
			result_type rst{};

			for(size_type y = 0; y < side_size; ++y){
				for(size_type x = 0; x < side_size; ++x){
					rst[x + y * side_size] = grid_generator::rectangle_at(diagonalAnchorPoints, x, y);
				}
			}

			return rst;
		}

		template <std::ranges::random_access_range Rng>
			requires (std::same_as<std::ranges::range_const_reference_t<Rng>, const Vector2D<T>&>)
		[[nodiscard]] static constexpr Vector2D<T> vertex_at(const Rng& anchorPoints, const size_type x, const size_type y) noexcept{
			return {anchorPoints[x].x, anchorPoints[y].y};
		}

		template <std::ranges::random_access_range Rng>
			requires (std::same_as<std::ranges::range_const_reference_t<Rng>, const Vector2D<T>&>)
		[[nodiscard]] static constexpr Rect_Orthogonal<T> rectangle_at(const Rng& anchorPoints, const size_type x, const size_type y) noexcept{
			auto v00 = grid_generator::vertex_at<Rng>(anchorPoints, x, y);
			auto v11 = grid_generator::vertex_at<Rng>(anchorPoints, x + 1, y + 1);

			CHECKED_ASSUME(v00.x <= v11.x);
			CHECKED_ASSUME(v00.y <= v11.y);

			return {v00, v11};
		}
	};

	template <std::size_t anchorPointCount, typename T, std::ranges::random_access_range Rng>
		requires requires{
			requires anchorPointCount >= 2;
			requires std::is_arithmetic_v<T>;
			requires std::convertible_to<std::ranges::range_value_t<Rng>, Vector2D<T>>;
		}
	struct deferred_grid_generator : std::ranges::view_interface<deferred_grid_generator<anchorPointCount, T, Rng>>, std::ranges::view_base{
		using generator_type = grid_generator<anchorPointCount, T>;
		using range_type = Rng;
		range_type arg{};

		[[nodiscard]] constexpr deferred_grid_generator() = default;

		[[nodiscard]] constexpr explicit deferred_grid_generator(const range_type& arg)
			: arg{arg}{}

		template <typename Ty>
			requires std::constructible_from<Rng, Ty>
		[[nodiscard]] constexpr explicit deferred_grid_generator(Ty&& arg)
			: arg{std::forward<Ty>(arg)}{}

		constexpr auto size() const noexcept{
			return generator_type::side_size * generator_type::side_size;
		}

		constexpr auto data() const noexcept = delete;

		struct iterator{
			const deferred_grid_generator* parent{};
			Geom::Vector2D<typename generator_type::size_type> cur{};

			using iterator_category = std::input_iterator_tag;
			using value_type = Rect_Orthogonal<T>;
			using difference_type = std::ptrdiff_t;
			using size_type = typename generator_type::size_type;

			constexpr friend bool operator==(const iterator& lhs, const iterator& rhs){
				assert(lhs.parent == rhs.parent);

				return lhs.cur == rhs.cur;
			}

			[[nodiscard]] constexpr value_type operator*() const noexcept{
				return generator_type::rectangle_at(parent->arg, cur.x, cur.y);
			}

			[[nodiscard]] constexpr auto operator->() const noexcept{
				return generator_type::vertex_at(parent->arg, cur.x, cur.y);
			}

			constexpr iterator& operator++() noexcept{
				assert(parent != nullptr);

				++cur.x;
				if(cur.x == generator_type::side_size){
					++cur.y;
					cur.x = 0;
				}

				assert(cur.x <= generator_type::side_size);
				assert(cur.y <= generator_type::side_size);

				return *this;
			}

			constexpr iterator operator++(int) noexcept{
				auto t = *this;
				++*this;
				return t;
			}
		};

		constexpr auto begin() const noexcept{
			return iterator{this, Geom::Vector2D<typename generator_type::size_type>{0, 0}};
		}

		constexpr auto end() const noexcept{
			return iterator{this, Geom::Vector2D<typename generator_type::size_type>{0, generator_type::side_size}};
		}
	};

	template <typename T, std::size_t size>
	deferred_grid_generator(std::array<Geom::Vector2D<T>, size>) -> deferred_grid_generator<size, T, std::ranges::views::all_t<std::array<Geom::Vector2D<T>, size>>>;

	template <std::size_t anchorPointCount = 4, typename T = float>
	requires (anchorPointCount >= 2 && std::is_arithmetic_v<T>)
	[[nodiscard]] decltype(auto) createGrid(const typename grid_generator<anchorPointCount, T>::input_type& diagonalAnchorPoints) noexcept {
		static constexpr grid_generator<anchorPointCount, T> generator{};
		return generator(diagonalAnchorPoints);
	}
}

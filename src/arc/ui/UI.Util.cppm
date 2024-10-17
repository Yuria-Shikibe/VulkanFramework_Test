//
// Created by Matrix on 2024/9/13.
//

export module Core.UI.Util;

import Geom.Vector2D;
import std;
import ext.concepts;
import ext.meta_programming;

namespace Core::UI::Util{

	template <typename Args, typename Fn, std::size_t ...I>
	constexpr auto setFunction_impl(Fn&& fn, std::index_sequence<I...>){
		return [fn = std::forward<Fn>(fn)](std::tuple_element_t<I, Args> ...){
			std::invoke(fn);
		};
	}

	export
	template <template<typename> typename FnWrap, typename FnTy, typename Fn>
		requires requires{ typename FnWrap<FnTy>; }
	void setFunction(FnWrap<FnTy>& func, Fn&& fn){
		using trait = ext::function_traits<FnTy>;

		if constexpr (ext::invocable<Fn, FnTy> && std::assignable_from<FnWrap<FnTy>&, Fn>){
			func = std::forward<Fn>(fn);
		}else if constexpr (std::invocable<Fn> && std::convertible_to<std::invoke_result_t<FnTy>, typename trait::return_type>){
			using seq = std::make_index_sequence<trait::args_count>;

			func = Util::setFunction_impl<trait::args_type>(std::forward<Fn>(fn), seq{});
		}else{
			static_assert(false, "unsupported function type");
		}
	}

	void foo(){
		// std::packaged_task<void(int, float)> fn{};
		//
		// static_assert(std::assignable_from<decltype(fn)&, decltype([](int, float){
		//
		// })>);
		//
		// setFunction(fn, [](int, float){
		//
		// });
	}

	export
	template <
		std::ranges::range Rng,
		std::predicate<std::ranges::range_const_reference_t<Rng>> Proj = std::identity
	>
		requires requires(Rng rng){
			std::ranges::rbegin(rng);
			std::ranges::empty(rng);
		}
	auto countRowAndColumn(Rng&& rng, Proj pred_isEndRow = {}){
		Geom::USize2 grid{};

		if(std::ranges::empty(rng)){
			return grid;
		}

		std::uint32_t curX{};
		auto itr = std::ranges::begin(rng);
		for(; itr != std::ranges::prev(std::ranges::end(rng)); ++itr){
			curX++;

			if(std::invoke(pred_isEndRow, itr)){
				grid.maxX(curX);
				grid.y++;
				curX = 0;
			}
		}

		grid.y++;
		grid.maxX(++curX);

		return grid;
	}

	export
	template <
		std::ranges::range Rng,
		std::predicate<std::ranges::range_const_reference_t<Rng>> Proj = std::identity
	>
	requires requires(Rng rng){
		std::ranges::rbegin(rng);
		std::ranges::empty(rng);
	}
	auto countRowAndColumn_toVector(Rng&& rng, Proj pred_isEndRow = {}){
		std::vector<std::uint32_t> grid{};

		if(std::ranges::empty(rng)){
			return grid;
		}

		std::uint32_t curX{};
		auto itr = std::ranges::begin(rng);
		for(; itr != std::ranges::prev(std::ranges::end(rng)); ++itr){
			curX++;

			if(std::invoke(pred_isEndRow, itr)){
				grid.push_back(curX);
				curX = 0;
			}
		}

		grid.push_back(++curX);

		return grid;
	}

	export
	float flipY(float height, const float height_in_valid, const float itemHeight){
		height = height_in_valid - height - itemHeight;
		return height;
	}

	export
	Geom::Vec2 flipY(Geom::Vec2 pos_in_valid, const float height_in_valid, const float itemHeight){
		pos_in_valid.y = flipY(pos_in_valid.y, height_in_valid, itemHeight);
		return pos_in_valid;
	}

	export
	/**
	 * @brief
	 * @return true if modification happens
	 */
	template <typename T>
		requires (std::equality_comparable<T> && std::is_move_assignable_v<T> && !std::is_trivial_v<T>)
	constexpr bool tryModify(T& target, T&& value) noexcept(noexcept(target == value) && std::is_nothrow_move_assignable_v<T>){
		if(target != value){
			target = std::move(value);
			return true;
		}
		return false;
	}

	export
	/**
	 * @brief
	 * @return true if modification happens
	 */
	template <typename T>
		requires (std::equality_comparable<T> && std::is_copy_assignable_v<T>)
	constexpr bool tryModify(T& target, const T& value) noexcept(noexcept(target == value) && std::is_nothrow_copy_assignable_v<T>){
		if(target != value){
			target = value;
			return true;
		}
		return false;
	}
}

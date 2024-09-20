//
// Created by Matrix on 2024/9/13.
//

export module Core.UI.Util;

import Geom.Vector2D;
import std;

export namespace Core::UI::Util{

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

	float flipY(float height, const float height_in_valid, const float itemHeight){
		height = height_in_valid - height - itemHeight;
		return height;
	}

	Geom::Vec2 flipY(Geom::Vec2 pos_in_valid, const float height_in_valid, const float itemHeight){
		pos_in_valid.y = flipY(pos_in_valid.y, height_in_valid, itemHeight);
		return pos_in_valid;
	}
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

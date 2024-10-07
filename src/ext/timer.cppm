//
// Created by Matrix on 2024/3/7.
//

export module ext.timer;

import std;

export namespace ext{

	/**
	 * @brief Time Unit in Game Tick! [60ticks / 1sec]
	 * @tparam size Interval Chennals
	 */
	template <typename T = float, std::size_t size = 1>
		requires (std::is_arithmetic_v<T>)
	class timer{

	public:
		using value_type = float;
	private:
		std::array<value_type, size> reloads{};

	public:
		static constexpr std::size_t timeSize = size;

		constexpr timer() noexcept = default;

		template <bool Strict = false, std::invocable<> Func>
		void update_and_run(std::size_t index, const value_type spacing, const value_type delta, Func func) noexcept(noexcept(std::invoke(func))){
			if(spacing == 0.0f){
				std::invoke(func);

				return;
			}

			auto& reload = reloads[index];
			reload += delta;

			if(reload >= spacing){
				if constexpr(Strict){
					const int total = static_cast<int>(std::ceil(reload / spacing));
					for(int i = 0; i < total; ++i){
						std::invoke(func);
					}
				} else{
					std::invoke(func);
				}

				reload = std::fmod(reload, spacing);
			}
		}

		constexpr bool update_and_get(std::size_t index, const value_type spacing, const value_type delta = 0) noexcept{
			auto& reload = reloads[index];
			reload += delta;

			if(reload >= delta){
				reload = std::fmod(reload, spacing);
				return true;
			}

			return false;
		}

		[[nodiscard]] constexpr bool get(std::size_t index, const value_type spacing) const noexcept{
			return reloads[index] > spacing;
		}

		constexpr void clear() noexcept{
			reloads.fill(value_type{});
		}

		constexpr void clear(std::size_t index) noexcept{
			reloads[index] = value_type{};
		}
	};
}

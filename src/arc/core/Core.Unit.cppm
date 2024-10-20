export module Core.Unit;

import std;
import Math;

export namespace Core{
	using TickRatio = std::ratio<1, 60>;

	using TimerSetter = double(*)();
	using DeltaSetter = double(*)(double);
	using TimeReseter = void(*)(double);

	template <typename T = float, typename Ratio = std::ratio<1>>
	struct DirectAccessTimeUnit : std::chrono::duration<T, Ratio>{
		using std::chrono::duration<T, Ratio>::count;
		using std::chrono::duration<T, Ratio>::rep;
		using std::chrono::duration<T, Ratio>::duration;

		[[nodiscard]] constexpr DirectAccessTimeUnit() noexcept = default;

		[[nodiscard]] constexpr explicit(false) DirectAccessTimeUnit(const T Val) noexcept
			: std::chrono::duration<T, Ratio>(Val){
		}

		template <typename T2, typename Ratio2>
		[[nodiscard]] constexpr explicit(false) DirectAccessTimeUnit(const DirectAccessTimeUnit<T2, Ratio2> other) noexcept
			: std::chrono::duration<T, Ratio>(std::chrono::duration_cast<std::chrono::duration<T, Ratio>>(other)){
		}

		[[nodiscard]] constexpr explicit(false) operator T() const noexcept{
			return this->count();
		}

		using std::chrono::duration<T, Ratio>::operator++;
		using std::chrono::duration<T, Ratio>::operator--;

		using std::chrono::duration<T, Ratio>::operator%=;

		using std::chrono::duration<T, Ratio>::operator+=;
		using std::chrono::duration<T, Ratio>::operator-=;
		using std::chrono::duration<T, Ratio>::operator*=;
		using std::chrono::duration<T, Ratio>::operator/=;

		using std::chrono::duration<T, Ratio>::operator+;
		using std::chrono::duration<T, Ratio>::operator-;

		constexpr DirectAccessTimeUnit& operator++() noexcept(std::is_arithmetic_v<rep>) /* strengthened */ {
			this->std::chrono::duration<T, Ratio>::operator++();
			return *this;
		}

		constexpr DirectAccessTimeUnit operator++(int) noexcept(std::is_arithmetic_v<rep>) /* strengthened */ {
			auto t = *this;
			this->operator++();
			return t;
		}

		constexpr DirectAccessTimeUnit& operator--() noexcept(std::is_arithmetic_v<rep>) /* strengthened */ {
			this->std::chrono::duration<T, Ratio>::operator--();
			return *this;
		}

		constexpr DirectAccessTimeUnit operator--(int) noexcept(std::is_arithmetic_v<rep>) /* strengthened */ {
			auto t = *this;
			this->operator--();
			return t;
		}

		friend T operator%(DirectAccessTimeUnit l, const T val) noexcept{
			return Math::mod<T>(T(l), val);
		}

		friend constexpr T operator+(DirectAccessTimeUnit l, const T val) noexcept{
			return T(l) + val;
		}

		friend constexpr T operator-(DirectAccessTimeUnit l, const T val) noexcept{
			return T(l) - val;
		}

		friend constexpr T operator*(DirectAccessTimeUnit l, const T val) noexcept{
			return T(l) * val;
		}

		friend constexpr T operator/(DirectAccessTimeUnit l, const T val) noexcept{
			return T(l) / val;
		}

		friend T operator%(const T val, DirectAccessTimeUnit l) noexcept{
			return Math::mod<T>(val, T(l));
		}

		friend constexpr T operator+(const T val, DirectAccessTimeUnit l) noexcept{
			return val + T(l);
		}

		friend constexpr T operator-(const T val, DirectAccessTimeUnit l) noexcept{
			return val - T(l);
		}

		friend constexpr T operator*(const T val, DirectAccessTimeUnit l) noexcept{
			return val * T(l);
		}

		friend constexpr T operator/(const T val, DirectAccessTimeUnit l) noexcept{
			return val / T(l);
		}
	};

	using Tick = DirectAccessTimeUnit<float, TickRatio>;
	using Sec = DirectAccessTimeUnit<float>;
	using Sec_Highres = DirectAccessTimeUnit<double>;

	// void foo(){
	// 	constexpr Sec sec{20};
	// 	constexpr Tick tick{ sec };
	// }
}
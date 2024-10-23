module;

#include <cassert>

export module Math.Angle;

import std;

#define CONSTEXPR /*constexpr*/

namespace Math{
	constexpr float HALF_VAL = 180.f;

	export struct FromRadiansTag{
	};

	export constexpr FromRadiansTag FromRadians{};

	/** @brief Uniformed Angle in [-180, 180] degree*/
	export struct Angle{
		using value_type = float;
		static constexpr value_type Pi = std::numbers::pi_v<value_type>;
		static constexpr value_type Bound = HALF_VAL;
		static constexpr value_type DegHalf = static_cast<value_type>(180);
		static constexpr value_type DegToRad = Pi / DegHalf;
		static constexpr value_type RadToDeg = DegHalf / Pi;

	private:
		value_type deg{};

		/**
		 * @return Angle in [0, 360] degree
		 */
		[[nodiscard]] static value_type getAngleInPi2(value_type rad) noexcept{
			rad = std::fmod(rad, HALF_VAL * 2.f);
			if(rad < 0) rad += HALF_VAL * 2.f;
			return rad;
		}

		/**
		 * @return Angle in [-180, 180] degree
		 */
		[[nodiscard]] static value_type getAngleInPi(value_type a) noexcept{
			a = getAngleInPi2(a);
			return a > HALF_VAL ? a - HALF_VAL * 2.f : a;
		}

		CONSTEXPR void clampInternal() noexcept{
			deg = getAngleInPi(deg);
		}

		constexpr void simpleClamp() noexcept{
			if (deg > Bound) {
				deg -= Bound * 2;
			} else if (deg < -Bound){
				deg += Bound * 2;
			}
		}

	public:
		[[nodiscard]] constexpr Angle() noexcept = default;

		[[nodiscard]] CONSTEXPR Angle(FromRadiansTag, const value_type deg) noexcept : deg(deg * RadToDeg){
			clampInternal();
		}

		[[nodiscard]] CONSTEXPR explicit(false) Angle(const value_type deg) noexcept : deg(deg){
			clampInternal();
		}

		constexpr void clamp(const value_type min, const value_type max) noexcept{
			deg = std::clamp(deg, min, max);
		}

		constexpr void clamp(const value_type maxabs) noexcept{
			assert(maxabs >= 0.f);
			deg = std::clamp(deg, -maxabs, maxabs);
		}

		constexpr friend bool operator==(const Angle& lhs, const Angle& rhs) noexcept = default;
		constexpr auto operator<=>(const Angle&) const noexcept = default;

		constexpr Angle operator-() const noexcept{
			Angle rst{*this};
			rst.deg = -rst.deg;
			return *this;
		}

		constexpr Angle& operator+=(const Angle other) noexcept{
			deg += other.deg;

			simpleClamp();

			return *this;
		}

		constexpr Angle& operator-=(const Angle other) noexcept{
			deg -= other.deg;

			simpleClamp();

			return *this;
		}

		CONSTEXPR Angle& operator*=(const value_type val) noexcept{
			deg *= val;
			clampInternal();

			return *this;
		}

		CONSTEXPR Angle& operator/=(const value_type val) noexcept{
			deg /= val;
			clampInternal();

			return *this;
		}

		CONSTEXPR Angle& operator%=(const value_type val) noexcept{
			deg = std::fmod(deg, val);

			return *this;
		}

		constexpr friend Angle operator+(Angle lhs, const Angle rhs) noexcept{
			return lhs += rhs;
		}

		constexpr friend Angle operator-(Angle lhs, const Angle rhs) noexcept{
			return lhs -= rhs;
		}

		CONSTEXPR friend Angle operator*(Angle lhs, const value_type rhs) noexcept{
			return lhs *= rhs;
		}

		CONSTEXPR friend Angle operator/(Angle lhs, const value_type rhs) noexcept{
			return lhs /= rhs;
		}

		CONSTEXPR friend Angle operator%(Angle lhs, const value_type rhs) noexcept{
			return lhs %= rhs;
		}

		constexpr explicit(false) operator value_type() const noexcept{
			return deg;
		}

		[[nodiscard]] constexpr value_type radians() const noexcept{
			return deg * DegToRad;
		}
	};
}

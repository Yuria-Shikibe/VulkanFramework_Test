//
// Created by Matrix on 2024/3/21.
//

export module Geom.Transform;

export import Geom.Vector2D;
export import Math.Angle;

import ext.concepts;
import std;

export namespace Geom{
	template <typename AngTy>
	struct TransformState {
		using AngleType = AngTy;

		Vec2 vec;
		AngleType rot;

		constexpr void setZero(){
			vec.setZero();
			rot = 0.0f;
		}

		template <float NaN = std::numeric_limits<AngleType>::signaling_NaN()>
		constexpr void setNan() noexcept requires std::is_floating_point_v<AngleType>{
			vec.set(NaN, NaN);
			rot = NaN;
		}

		constexpr TransformState& applyInv(const TransformState debaseTarget) noexcept{
			vec.sub(debaseTarget.vec).rotate(-debaseTarget.rot);
			rot -= debaseTarget.rot;

			return *this;
		}

		constexpr TransformState& apply(const TransformState rebaseTarget) noexcept{
			vec.rotate(static_cast<float>(rebaseTarget.rot)).add(rebaseTarget.vec);
			rot += rebaseTarget.rot;

			return *this;
		}

		constexpr TransformState& operator|=(const TransformState parentRef) noexcept{
			return TransformState::apply(parentRef);
		}

		constexpr TransformState& operator+=(const TransformState other) noexcept{
			vec += other.vec;
			rot += other.rot;

			return *this;
		}

		constexpr TransformState& operator-=(const TransformState other) noexcept{
			vec -= other.vec;
			rot -= other.rot;

			return *this;
		}

		constexpr TransformState& operator*=(const float scl) noexcept{
			vec *= scl;
			rot *= scl;

			return *this;
		}

		[[nodiscard]] constexpr friend TransformState operator*(TransformState self, const float scl) noexcept{
			return self *= scl;
		}

		[[nodiscard]] constexpr friend TransformState operator+(TransformState self, const TransformState other) noexcept{
			return self += other;
		}

		[[nodiscard]] constexpr friend TransformState operator-(TransformState self, const TransformState other) noexcept{
			return self -= other;
		}

		[[nodiscard]] constexpr friend TransformState operator-(TransformState self) noexcept{
			self.vec.reverse();
			self.rot *= -1;
			return self;
		}

		template <typename T>
			requires (std::convertible_to<AngleType, T>)
		constexpr explicit(false) operator TransformState<T>() noexcept{
			return TransformState<T>{vec, static_cast<T>(rot)};
		}

		/**
		 * @brief Local To Parent
		 * @param self To Trans
		 * @param parentRef Parent Frame Reference Trans
		 * @return Transformed Translation
		 */
		[[nodiscard]] constexpr friend TransformState operator|(TransformState self, const TransformState parentRef) noexcept{
			return self |= parentRef;
		}

		[[nodiscard]] constexpr friend Vec2& operator|=(Vec2& vec, const TransformState transform) noexcept{
			return vec.rotate(static_cast<float>(transform.rot)).add(transform.vec);
		}

		[[nodiscard]] constexpr friend Vec2 operator|(Vec2 vec, const TransformState transform) noexcept{
			return vec |= transform;
		}
	};

	using Transform = TransformState<float>;
	using UniformTransform = TransformState<Math::Angle>;
}

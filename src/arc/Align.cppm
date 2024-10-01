export module Align;

import ext.concepts;
export import Geom.Vector2D;
export import Geom.Rect_Orthogonal;

import std;
import Math;

namespace Align{
	template <ext::number T1, ext::number T2>
	constexpr float floating_div(const T1 a, const T2 b) noexcept{
		return static_cast<float>(a) / static_cast<float>(b);
	}

	template <ext::number T1>
	constexpr T1 floating_mul(const T1 a, const float b) noexcept{
		if constexpr (std::is_floating_point_v<T1>){
			return a * b;
		}else{
			return Math::round<T1>(static_cast<float>(a) * b);
		}
	}
}

export namespace Align{
	template<typename T>
		requires (std::is_arithmetic_v<T>)
	struct Pad{
		/**@brief Left Spacing*/
		T left{};
		/**@brief Right Spacing*/
		T right{};
		/**@brief Bottom Spacing*/
		T bottom{};
		/**@brief Top Spacing*/
		T top{};

		[[nodiscard]] constexpr Geom::Vector2D<T> bot_lft() const noexcept{
			return {left, bottom};
		}

		[[nodiscard]] constexpr Geom::Vector2D<T> top_rit() const noexcept{
			return {right, top};
		}

		[[nodiscard]] constexpr Geom::Vector2D<T> top_lft() const noexcept{
			return {left, top};
		}

		[[nodiscard]] constexpr Geom::Vector2D<T> bot_rit() const noexcept{
			return {right, bottom};
		}

		[[nodiscard]] friend constexpr bool operator==(const Pad& lhs, const Pad& rhs) noexcept = default;

		constexpr void expand(T x, T y) noexcept{
			x *= 0.5f;
			y *= 0.5f;

			left += x;
			right += x;
			top += y;
			bottom += y;
		}

		constexpr void expand(const T val) noexcept{
			this->expand(val, val);
		}

		[[nodiscard]] constexpr T getWidth() const noexcept{
			return left + right;
		}

		[[nodiscard]] constexpr T getHeight() const noexcept{
			return bottom + top;
		}

		[[nodiscard]] constexpr Geom::Vector2D<T> getSize() const noexcept{
			return {getWidth(), getHeight()};
		}

		[[nodiscard]] constexpr T getRemainWidth(const T total = 1) const noexcept{
			return total - getWidth();
		}

		[[nodiscard]] constexpr T getRemainHeight(const T total = 1) const noexcept{
			return total - getHeight();
		}

		constexpr Pad& set(const T val) noexcept{
			bottom = top = left = right = val;
			return *this;
		}

		constexpr Pad& set(const T l, const T r, const T b, const T t) noexcept{
			left = l;
			right = r;
			bottom = b;
			top = t;
			return *this;
		}

		constexpr Pad& setZero() noexcept{
			return set(0);
		}
	};

	template<typename T>
	Pad<T> padBetween(const Geom::Rect_Orthogonal<T>& internal, const Geom::Rect_Orthogonal<T>& external){
		return Pad<T>{
			internal.getSrcX() - external.getSrcX(),
			external.getEndX() - internal.getEndX(),
			internal.getSrcY() - external.getSrcY(),
			external.getEndY() - internal.getEndY(),
		};
	}

	using Spacing = Pad<float>;

	enum class Pos : unsigned char{
		left     = 0b0000'0001,
		right    = 0b0000'0010,
		center_x = 0b0000'0100,

		top      = 0b0000'1000,
		bottom   = 0b0001'0000,
		center_y = 0b0010'0000,

		top_left   = top | left,
		top_center = top | center_x,
		top_right  = top | right,

		center_left  = center_y | left,
		center       = center_y | center_x,
		center_right = center_y | right,

		bottom_left   = bottom | left,
		bottom_center = bottom | center_x,
		bottom_right  = bottom | right,
	};

	enum class Scale : unsigned char{
		/** The source is not scaled. */
		none,

		/**
		 * Scales the source to fit the target while keeping the same aspect ratio. This may cause the source to be smaller than the
		 * target in one direction.
		 */
		fit,

		/**
		 * Scales the source to fit the target if it is larger, otherwise does not scale.
		 */
		bounded,

		/**
		 * Scales the source to fill the target while keeping the same aspect ratio. This may cause the source to be larger than the
		 * target in one direction.
		 */
		fill,

		/**
		 * Scales the source to fill the target in the x direction while keeping the same aspect ratio. This may cause the source to be
		 * smaller or larger than the target in the y direction.
		 */
		fillX,

		/**
		 * Scales the source to fill the target in the y direction while keeping the same aspect ratio. This may cause the source to be
		 * smaller or larger than the target in the x direction.
		 */
		fillY,

		/** Scales the source to fill the target. This may cause the source to not keep the same aspect ratio. */
		stretch,

		/**
		 * Scales the source to fill the target in the x direction, without changing the y direction. This may cause the source to not
		 * keep the same aspect ratio.
		 */
		stretchX,

		/**
		 * Scales the source to fill the target in the y direction, without changing the x direction. This may cause the source to not
		 * keep the same aspect ratio.
		 */
		stretchY,
	};

	template <ext::number T>
	constexpr Geom::Vector2D<T> embedTo(const Scale stretch, Geom::Vector2D<T> srcSize, Geom::Vector2D<T> toBound){
		switch(stretch){
			case Scale::fit :{
				const float targetRatio = Align::floating_div(toBound.y, toBound.x);
				const float sourceRatio =
					Align::floating_div(srcSize.y, srcSize.x);
				float scale = targetRatio > sourceRatio ?
					Align::floating_div(toBound.x, srcSize.x) :
					Align::floating_div(toBound.y, srcSize.y);

				return {Align::floating_mul<T>(srcSize.x, scale), Align::floating_mul<T>(srcSize.y, scale)};
			}
			case Scale::fill :{
				const float targetRatio =
					Align::floating_div(toBound.y, toBound.x);
				const float sourceRatio =
					Align::floating_div(srcSize.y, srcSize.x);
				float scale = targetRatio < sourceRatio ?
					Align::floating_div(toBound.x, srcSize.x) :
					Align::floating_div(toBound.y, srcSize.y);

				return {Align::floating_mul<T>(srcSize.x, scale), Align::floating_mul<T>(srcSize.y, scale)};
			}
			case Scale::fillX :{
				float scale = Align::floating_div(toBound.x, srcSize.x);
				return {Align::floating_mul<T>(srcSize.x, scale), Align::floating_mul<T>(srcSize.y, scale)};
			}
			case Scale::fillY :{
				float scale = Align::floating_div(toBound.y, srcSize.y);
				return {Align::floating_mul<T>(srcSize.x, scale), Align::floating_mul<T>(srcSize.y, scale)};
			}
			case Scale::stretch : return toBound;
			case Scale::stretchX : return {toBound.x, srcSize.y};
			case Scale::stretchY : return {srcSize.x, toBound.y};
			case Scale::bounded :
				if(srcSize.y > toBound.y || srcSize.x > toBound.x){
					return Align::embedTo<T>(Scale::fit, srcSize, toBound);
				} else{
					return Align::embedTo<T>(Scale::none, srcSize, toBound);
				}
			case Scale::none :
				return srcSize;
		}
		
		std::unreachable();
	}

	constexpr bool operator &(const Pos l, const Pos r){
		return std::to_underlying(l) & std::to_underlying(r);
	}

	template <ext::signed_number T>
	constexpr Geom::Vec2 getOffsetOf(const Pos align, const Geom::Vector2D<T> bottomLeft, const Geom::Vector2D<T> topRight){
		Geom::Vector2D<T> move{};

		if(align & Pos::top){
			move.y = -topRight.y;
		} else if(align & Pos::bottom){
			move.y = bottomLeft.y;
		}

		if(align & Pos::right){
			move.x = -topRight.x;
		} else if(align & Pos::left){
			move.x = bottomLeft.x;
		}

		return move;
	}

	constexpr Geom::Vec2 getOffsetOf(const Pos align, const Spacing margin){
		float xMove = 0;
		float yMove = 0;

		if(align & Pos::top){
			yMove = -margin.top;
		} else if(align & Pos::bottom){
			yMove = margin.bottom;
		}

		if(align & Pos::right){
			xMove = -margin.right;
		} else if(align & Pos::left){
			xMove = margin.left;
		}

		return {xMove, yMove};
	}


	/**
	 * @brief
	 * @tparam T arithmetic type, does not accept unsigned type
	 * @return
	 */
	template <ext::signed_number T>
	constexpr Geom::Vector2D<T> getOffsetOf(const Pos align, const Geom::Rect_Orthogonal<T>& bound){
		Geom::Vector2D<T> offset{};

		if(align & Pos::top){
			offset.y = -bound.getHeight();
		} else if(align & Pos::center_y){
			offset.y = -bound.getHeight() / static_cast<T>(2);
		}

		if(align & Pos::right){
			offset.x = -bound.getWidth();
		} else if(align & Pos::center_x){
			offset.x = -bound.getWidth() / static_cast<T>(2);
		}

		return offset;
	}

	/**
	 * @brief
	 * @tparam T arithmetic type, does not accept unsigned type
	 * @return
	 */
	template <ext::signed_number T>
	constexpr Geom::Vector2D<T> getOffsetOf(const Pos align, const Geom::Vector2D<T>& bound){
		return Align::getOffsetOf(align, Geom::Rect_Orthogonal<T>{bound});
	}

	/**
	 * @brief
	 * @tparam T arithmetic type, does not accept unsigned type
	 * @return
	 */
	template <ext::signed_number T>
	constexpr Geom::Vector2D<T> getVert(const Pos align, const Geom::Rect_Orthogonal<T>& bound){
		Geom::Vector2D<T> offset{bound.getSrc()};


		if(align & Pos::top){
			offset.y = bound.getEndY();
		} else if(align & Pos::center_y){
			offset.y += bound.getHeight() / static_cast<T>(2);
		}

		if(align & Pos::right){
			offset.x = bound.getEndX();
		} else if(align & Pos::center_x){
			offset.x += bound.getWidth() / static_cast<T>(2);
		}

		return offset;
	}

	/**
	 * @brief
	 * @tparam T arithmetic type, does not accept unsigned type
	 * @return
	 */
	template <ext::signed_number T>
	[[nodiscard]] constexpr Geom::Vector2D<T> getOffsetOf(
		const Pos align,
		const typename Geom::Vector2D<T>::PassType internal_toAlignSize,
		const Geom::Rect_Orthogonal<T>& external){
		Geom::Vector2D<T> offset{};

		if(align & Pos::top){
			offset.y = external.getEndY() - internal_toAlignSize.y;
		} else if(align & Pos::bottom){
			offset.y = external.getSrcY();
		} else{
			//centerY
			offset.y = external.getSrcY() + (external.getHeight() - internal_toAlignSize.y) / static_cast<T>(2);
		}

		if(align & Pos::right){
			offset.x = external.getEndX() - internal_toAlignSize.x;
		} else if(align & Pos::left){
			offset.x = external.getSrcX();
		} else{
			//centerX
			offset.x = external.getSrcX() + (external.getWidth() - internal_toAlignSize.x) / static_cast<T>(2);
		}

		return offset;
	}


	/**
	 * @brief
	 * @return still offset to bottom left, but aligned to given align within the bound
	 */
	template <ext::signed_number T>
	constexpr Geom::Vector2D<T> getTransformedOffsetOf(
		const Pos align,
		Geom::Vector2D<T> scale
	){

		if(align & Pos::top){
			scale.y *= -1;
		}else if(align & Pos::center_y){
			scale.y = 0;
		}

		if(align & Pos::right){
			scale.x *= -1;
		}else if(align & Pos::center_x){
			scale.x = 0;
		}

		return scale;
	}


	/**
	 * @brief
	 * @tparam T arithmetic type, does not accept unsigned type
	 * @return
	 */
	template <ext::signed_number T>
	constexpr Geom::Vector2D<T> getOffsetOf(const Pos align, const Geom::Rect_Orthogonal<T>& internal_toAlign,
	                                        const Geom::Rect_Orthogonal<T>& external){
		return Align::getOffsetOf(align, internal_toAlign.getSize(), external);
	}
}

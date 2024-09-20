//
// Created by Matrix on 2024/9/13.
//

export module Core.UI.ClampedSize;

export import Geom.Vector2D;
import Core.UI.Util;
import std;

export namespace Core::UI{
	struct ClampedSize{
		using T = float;
		using SizeType = Geom::Vector2D<T>;
	private:
		SizeType minimumSize{Geom::zeroVec2<T>};
		SizeType maximumSize{Geom::maxVec2<T>};
		SizeType size{};

	public:
		[[nodiscard]] constexpr ClampedSize() = default;

		[[nodiscard]] constexpr explicit ClampedSize(const SizeType& size)
			: size{size}{}

		[[nodiscard]] constexpr ClampedSize(const SizeType& minimumSize, const SizeType& maximumSize, const SizeType& size)
			: minimumSize{minimumSize},
			  maximumSize{maximumSize},
			  size{size}{}

		[[nodiscard]] constexpr SizeType getMinimumSize() const noexcept{
			return minimumSize;
		}

		[[nodiscard]] constexpr SizeType getMaximumSize() const noexcept{
			return maximumSize;
		}

		[[nodiscard]] constexpr SizeType getSize() const noexcept{
			return size;
		}

		[[nodiscard]] constexpr T getWidth() const noexcept{
			return size.x;
		}

		[[nodiscard]] constexpr T getHeight() const noexcept{
			return size.y;
		}

		constexpr void setSize_unchecked(const SizeType size) noexcept{
			this->size = size;
		}

		/**
		 * @brief
		 * @return true if width has been changed
		 */
		constexpr bool setWidth(T w) noexcept{
			w = std::clamp(w, minimumSize.x, maximumSize.x);
			return Util::tryModify(size.x, w);
		}

		/**
		 * @brief
		 * @return true if height has been changed
		 */
		constexpr bool setHeight(T h) noexcept{
			h = std::clamp(h, minimumSize.y, maximumSize.y);
			return Util::tryModify(size.y, h);
		}

		/**
		 * @brief
		 * @return true if size has been changed
		 */
		constexpr bool setSize(const SizeType s) noexcept{
			bool b{};
			b |= setWidth(s.x);
			b |= setHeight(s.y);
			return b;
		}

		/**
		 * @brief
		 * @return true if size has been changed
		 */
		constexpr bool setMinimumSize(SizeType sz) noexcept{
			if(this->minimumSize != sz){
				this->minimumSize = sz;
				return Util::tryModify(size, sz.max(size));
			}
			return false;
		}

		/**
		 * @brief
		 * @return true if size has been changed
		 */
		constexpr bool setMaximumSize(SizeType sz) noexcept{
			if(this->maximumSize != sz){
				this->maximumSize = sz;
				return Util::tryModify(size, sz.min(size));
			}
			return false;
		}

		constexpr friend bool operator==(const ClampedSize& lhs, const ClampedSize& rhs) = default;
	};
}

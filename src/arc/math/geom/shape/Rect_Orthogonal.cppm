module;

// #include "../src/ext/assume.hpp"

export module Geom.Rect_Orthogonal;

import std;

import ext.concepts;
import Math;
import Geom.Vector2D;

export namespace Geom{

	struct FromExtentTag{};
	constexpr FromExtentTag FromExtent{};

	//TODO make size as a vector?

	/**
	 * \brief width, height should be always non-negative.
	 * \tparam T Arithmetic Type
	 */
	template <ext::number T>
	class Rect_Orthogonal/* : public Shape<Rect_Orthogonal<T>, T>*/{
		static constexpr T TWO{2};

	public:
		Vector2D<T> src{};
		
	private:
		Vector2D<T> size_{};

	public:
		constexpr Rect_Orthogonal(const T srcX, const T srcY, const T width, const T height) noexcept
			: src(srcX, srcY){
			this->setSize(width, height);
		}

		constexpr Rect_Orthogonal(const typename Vector2D<T>::PassType center, const T width, const T height) noexcept{
			this->setSize(width, height);
			this->setCenter(center.x, center.y);
		}

		constexpr Rect_Orthogonal(const T width, const T height) noexcept{
			this->setSize(width, height);
		}

		constexpr explicit Rect_Orthogonal(const typename Vector2D<T>::PassType size) noexcept{
			this->setSize(size);
		}

		constexpr Rect_Orthogonal(const typename Vector2D<T>::PassType center, const T size) noexcept{
			this->setSize(size, size);
			this->setCenter(center.x, center.y);
		}

		/**
		 * @warning Create by vertex [src, end] instead of [src, size]
		 */
		constexpr Rect_Orthogonal(const typename Vector2D<T>::PassType src, const typename Vector2D<T>::PassType end) noexcept{
			this->setVert(src, end);
		}

		/**
		 * @brief Create by [src, size]
		 */
		constexpr Rect_Orthogonal(FromExtentTag, const typename Vector2D<T>::PassType src, const typename Vector2D<T>::PassType size) noexcept
			: src(src){
			this->setSize(size);
		}

		constexpr Rect_Orthogonal(FromExtentTag,
			const typename Vector2D<T>::PassType src, const T width, const T height) noexcept
			: src(src){
			this->setSize(width, height);
		}

		constexpr explicit Rect_Orthogonal(const T size) noexcept{
			this->setSize(size, size);
		}

		constexpr Rect_Orthogonal() noexcept = default;

		friend constexpr bool operator==(const Rect_Orthogonal& lhs, const Rect_Orthogonal& rhs) noexcept = default;

		friend std::ostream& operator<<(std::ostream& os, const Rect_Orthogonal& obj) noexcept{
			return os << std::format("[{}, {}, {}, {}]", obj.src.x, obj.src.y, obj.size_.x, obj.size_.y);
		}

		[[nodiscard]] constexpr T getSrcX() const noexcept{
			return src.x;
		}

		[[nodiscard]] constexpr Geom::Vector2D<T> getSrc() const noexcept{
			return src;
		}

		[[nodiscard]] constexpr Geom::Vector2D<T> getEnd() const noexcept{
			return {getEndX(), getEndY()};
		}


		Rect_Orthogonal& expandBy(const Rect_Orthogonal& other) noexcept{
			Geom::Vector2D<T> min{Math::min(getSrcX(), other.getSrcX()), Math::min(getSrcY(), other.getSrcY())};
			Geom::Vector2D<T> max{Math::max(getEndX(), other.getEndX()), Math::max(getEndY(), other.getEndY())};

			this->setVert(min, max);

			return *this;
		}

		[[nodiscard]] constexpr T getSrcY() const noexcept{
			return src.y;
		}

		[[nodiscard]] constexpr T getWidth() const noexcept{
			return size_.x;
		}

		[[nodiscard]] constexpr Vector2D<T> getSize() const noexcept{
			return size_;
		}

		[[nodiscard]] constexpr T getHeight() const noexcept{
			return size_.y;
		}

		template <ext::number T_>
		constexpr Rect_Orthogonal<T_> as() const noexcept{
			return Rect_Orthogonal<T_>{
				static_cast<T_>(src.x),
				static_cast<T_>(src.y),
				static_cast<T_>(size_.x),
				static_cast<T_>(size_.y),
			};
		}


		[[nodiscard]] constexpr auto asSigned() const noexcept{
			if constexpr (std::is_unsigned_v<T>){
				using S = std::make_signed_t<T>;
				return Rect_Orthogonal<S>{
					static_cast<S>(src.x  ),
					static_cast<S>(src.y  ),
					static_cast<S>(size_.x),
					static_cast<S>(size_.y)
				};
			}else{
				return *this;
			}
		}

		template <ext::number N>
		constexpr auto& setWidth(const N w) noexcept{
			if constexpr(std::is_unsigned_v<N>) {
				size_.x = static_cast<T>(w);
			}else {
				if(w >= 0){
					size_.x = static_cast<T>(w);
				}else{
					T abs = -static_cast<T>(w);
					src.x -= abs;
					size_.x = abs;
				}
			}

			return *this;
		}

		template <ext::number N>
		constexpr auto& setHeight(const N h) noexcept{
			if constexpr(std::is_unsigned_v<N>) {
				size_.y = static_cast<T>(h);
			}else {
				if(h >= 0){
					size_.y = static_cast<T>(h);
				}else{
					T abs = -static_cast<T>(h);
					src.y -= abs;
					size_.y = abs;
				}
			}

			return *this;
		}

		constexpr Rect_Orthogonal& addSize(const T x, const T y) noexcept requires (ext::signed_number<T>) {
			this->setWidth<T>(size_.x + x);
			this->setHeight<T>(size_.y + y);

			return *this;
		}

		constexpr Rect_Orthogonal& addWidth(const T x) noexcept{
			this->setWidth<T>(size_.x + x);

			return *this;
		}

		constexpr Rect_Orthogonal& addHeight(const T y) noexcept{
			this->setHeight<T>(size_.y + y);

			return *this;
		}

		constexpr Rect_Orthogonal& shrinkBy(const typename Vector2D<T>::PassType directionAndSize) noexcept{
			const T minX = Math::min(directionAndSize.x, size_.x);
			size_.x -= minX;

			if(directionAndSize.x > 0){
				src.x += minX;
			}

			const T minY = Math::min(directionAndSize.y, size_.y);
			size_.y -= minY;

			if(directionAndSize.y > 0){
				src.y += minY;
			}

			return *this;
		}

		template <ext::number N>
		constexpr Rect_Orthogonal& addSize(const N x, const N y) noexcept{
			using S = std::make_signed_t<T>;
			this->setWidth<S>(static_cast<S>(size_.x) + static_cast<S>(x));
			this->setHeight<S>(static_cast<S>(size_.y) + static_cast<S>(y));

			return *this;
		}

		constexpr void setLargerWidth(const T v) noexcept{
			if constexpr(std::is_unsigned_v<T>) {
				if(v > size_.x) {
					this->setWidth<T>(v);
				}
			}else {
				T abs = static_cast<T>(v < 0 ? -v : v);
				if(abs > size_.x) {
					this->setWidth<T>(v);
				}
			}

		}

		constexpr void setLargerHeight(const T v) noexcept{
			if constexpr(std::is_unsigned_v<T>) {
				if(v > size_.y) {
					this->setHeight<T>(v);
				}
			}else {
				T abs = static_cast<T>(v < 0 ? -v : v);
				if(abs > size_.y) {
					this->setHeight<T>(v);
				}
			}
		}

		constexpr void setShorterWidth(const T v) noexcept{
			if constexpr(std::is_unsigned_v<T>) {
				if(v < size_.x) {
					this->setWidth<T>(v);
				}
			}else {
				T abs = static_cast<T>(v < 0 ? -v : v);
				if(abs < size_.x) {
					this->setWidth<T>(v);
				}
			}

		}

		constexpr void setShorterHeight(const T v) noexcept{
			if constexpr(std::is_unsigned_v<T>) {
				if(v < size_.y) {
					this->setHeight<T>(v);
				}
			}else {
				T abs = static_cast<T>(v < 0 ? -v : v);
				if(abs < size_.y) {
					this->setHeight<T>(v);
				}
			}
		}

		[[nodiscard]] constexpr bool containsStrict(const Rect_Orthogonal& other) const noexcept{
			return
				other.src.x > src.x && other.src.x + other.size_.x < src.x + size_.x &&
				other.src.y > src.y && other.src.y + other.size_.y < src.y + size_.y;
		}

		[[nodiscard]] constexpr bool containsLoose(const Rect_Orthogonal& other) const noexcept{
			return
				other.src.x >= src.x && other.getEndX() <= getEndX() &&
				other.src.y >= src.y && other.getEndY() <= getEndY();
		}

		[[nodiscard]] constexpr bool contains(const Rect_Orthogonal& other) const noexcept{
			return
				other.src.x >= src.x && other.getEndX() < getEndX() &&
				other.src.y >= src.y && other.getEndY() < getEndY();
		}

		[[nodiscard]] constexpr bool overlap_Exclusive(const Rect_Orthogonal& r) const noexcept{
			return
				getSrcX() < r.getEndX() &&
				getEndX() > r.getSrcX() &&
				getSrcY() < r.getEndY() &&
				getEndY() > r.getSrcY();
		}

		[[nodiscard]] constexpr bool overlap_Inclusive(const Rect_Orthogonal& r) const noexcept{
			return
				getSrcX() <= r.getEndX() &&
				getEndX() >= r.getSrcX() &&
				getSrcY() <= r.getEndY() &&
				getEndY() >= r.getSrcY();
		}

		[[nodiscard]] constexpr bool overlap_Exclusive(const Rect_Orthogonal& r, const typename Vector2D<T>::PassType selfTrans, const typename Vector2D<T>::PassType otherTrans) const noexcept{
			return
				getSrcX() + selfTrans.x < r.getEndX() + otherTrans.x &&
				getEndX() + selfTrans.x > r.getSrcX() + otherTrans.x &&
				getSrcY() + selfTrans.y < r.getEndY() + otherTrans.y &&
				getEndY() + selfTrans.y > r.getSrcY() + otherTrans.y;
		}

		[[nodiscard]] constexpr bool containsPos_edgeExclusive(const typename Vector2D<T>::PassType v) const noexcept{
			return v.x > src.x && v.y > src.y && v.x < getEndX() && v.y < getEndY();
		}

		[[nodiscard]] constexpr bool containsPos_edgeInclusive(const typename Vector2D<T>::PassType v) const noexcept{
			return v.x >= src.x && v.y >= src.y && v.x < getEndX() && v.y < getEndY();
		}

		[[nodiscard]] constexpr T getEndX() const noexcept{
			return src.x + size_.x;
		}

		[[nodiscard]] constexpr T getEndY() const noexcept{
			return src.y + size_.y;
		}

		[[nodiscard]] constexpr T getCenterX() const noexcept{
			return src.x + size_.x / TWO;
		}

		[[nodiscard]] constexpr T getCenterY() const noexcept{
			return src.y + size_.y / TWO;
		}

		[[nodiscard]] constexpr Vector2D<T> getCenter() const noexcept{
			return {getCenterX(), getCenterY()};
		}

		[[nodiscard]] constexpr T maxDiagonalSqLen() const noexcept{
			return size_.x * size_.x + size_.y * size_.y;
		}

		constexpr Rect_Orthogonal& setSrc(const T x, const T y) noexcept{
			src.x = x;
			src.y = y;

			return *this;
		}

		constexpr Rect_Orthogonal& setEnd(const T x, const T y) noexcept{
			this->setWidth(x - src.x);
			this->setHeight(y - src.y);

			return *this;
		}

		constexpr Rect_Orthogonal& setEndX(const T x) noexcept{
			this->setWidth(x - src.x);

			return *this;
		}

		constexpr Rect_Orthogonal& setEndY(const T y) noexcept{
			this->setHeight(y - src.y);

			return *this;
		}

		constexpr Rect_Orthogonal& setSrc(const typename Vector2D<T>::PassType v) noexcept{
			src = v;
			return *this;
		}

		constexpr Rect_Orthogonal& setSize(const T x, const T y) noexcept{
			this->setWidth(x);
			this->setHeight(y);

			return *this;
		}

		constexpr Rect_Orthogonal& setSize(const Rect_Orthogonal& other) noexcept{
			this->setWidth(other.size_.x);
			this->setHeight(other.size_.y);

			return *this;
		}

		constexpr Rect_Orthogonal& moveY(const T y) noexcept{
			src.y += y;

			return *this;
		}

		constexpr Rect_Orthogonal& moveX(const T x) noexcept{
			src.x += x;

			return *this;
		}

		constexpr Rect_Orthogonal& move(const T x, const T y) noexcept{
			src.x += x;
			src.y += y;

			return *this;
		}

		constexpr Rect_Orthogonal& move(const typename Geom::Vector2D<T>::PassType vec) noexcept{
			src += vec;

			return *this;
		}

		constexpr Rect_Orthogonal& setSrc(const Rect_Orthogonal& other) noexcept{
			src = other.src;

			return *this;
		}

		template <ext::number T1, ext::number T2>
		constexpr Rect_Orthogonal& sclSize(const T1 xScl, const T2 yScl) noexcept{
			size_.x = static_cast<T>(static_cast<T1>(size_.x) * xScl);
			size_.y = static_cast<T>(static_cast<T1>(size_.y) * yScl);

			return *this;
		}

		template <ext::number T1, ext::number T2>
		constexpr Rect_Orthogonal& sclPos(const T1 xScl, const T2 yScl) noexcept{
			src.x = static_cast<T>(static_cast<T1>(src.x) * xScl);
			src.y = static_cast<T>(static_cast<T1>(src.y) * yScl);

			return *this;
		}

		template <ext::number T1, ext::number T2>
		constexpr Rect_Orthogonal& scl(const T1 xScl, const T2 yScl) noexcept{
			(void)this->sclPos<T1, T2>(xScl, yScl);
			(void)this->sclSize<T1, T2>(xScl, yScl);

			return *this;
		}

		template <ext::number N>
		constexpr Rect_Orthogonal& scl(const typename Vector2D<N>::PassType scl) noexcept{
			(void)this->sclPos<N, N>(scl.x, scl.y);
			(void)this->sclSize<N, N>(scl.x, scl.y);

			return *this;
		}

		constexpr void set(const T srcx, const T srcy, const T width, const T height) noexcept{
			src.x = srcx;
			src.y = srcy;

			this->setWidth<T>(width);
			this->setHeight<T>(height);
		}

		template <std::integral N>
		Rect_Orthogonal<N> trac() noexcept{
			return Rect_Orthogonal<N>{Math::trac<N>(src.x), Math::trac<N>(src.y), Math::trac<N>(size_.x), Math::trac<N>(size_.y)};
		}

		template <std::integral N>
		Rect_Orthogonal<N> round() noexcept{
			return Rect_Orthogonal<N>{Math::round<N>(src.x), Math::round<N>(src.y), Math::round<N>(size_.x), Math::round<N>(size_.y)};
		}

		constexpr Rect_Orthogonal& setSize(const typename Vector2D<T>::PassType v) noexcept{
			return this->setSize(v.x, v.y);
		}

		constexpr Rect_Orthogonal& setCenter(const T x, const T y) noexcept{
			this->setSrc(x - size_.x / TWO, y - size_.y / TWO);

			return *this;
		}

		constexpr Rect_Orthogonal& setCenter(const typename Vector2D<T>::PassType v) noexcept{
			this->setSrc(v.x - size_.x / TWO, v.y - size_.y / TWO);

			return *this;
		}

		[[nodiscard]] constexpr float xOffsetRatio(const T x) const noexcept{
			return Math::curve(x, static_cast<float>(src.x), static_cast<float>(src.x + size_.x));
		}

		[[nodiscard]] constexpr float yOffsetRatio(const T y) const noexcept{
			return Math::curve(y, static_cast<float>(src.y), static_cast<float>(src.y + size_.y));
		}

		[[nodiscard]] constexpr Vec2 offsetRatio(const Vec2& v) noexcept{
			return { xOffsetRatio(v.x), yOffsetRatio(v.y) };
		}

		[[nodiscard]] constexpr Vector2D<T> vert_00()const noexcept{
			return src;
		}

		[[nodiscard]] constexpr Vector2D<T> vert_10() const noexcept{
			return { src.x + size_.x, src.y };
		}

		[[nodiscard]] constexpr Vector2D<T> vert_01() const noexcept{
			return { src.x, src.y + size_.y };
		}

		[[nodiscard]] constexpr Vector2D<T> vert_11() const noexcept{
			return { src.x + size_.x, src.y + size_.y };
		}

		[[nodiscard]] constexpr T area() const noexcept{
			return size_.x * size_.y;
		}

		[[nodiscard]] constexpr float ratio() const noexcept{
			return static_cast<float>(size_.x) / static_cast<float>(size_.y);
		}

		constexpr Rect_Orthogonal& setVert(const T srcX, const T srcY, const T endX, const T endY) noexcept{
			auto [minX, maxX] = Math::minmax(srcX, endX);
			auto [minY, maxY] = Math::minmax(srcY, endY);
			this->src.x = minX;
			this->src.y = minY;
			size_.x = maxX - minX;
			size_.y = maxY - minY;

			return *this;
		}

		constexpr Rect_Orthogonal& setVert(const typename Vector2D<T>::PassType src, const typename Vector2D<T>::PassType end) noexcept{
			return this->setVert(src.x, src.y, end.x, end.y);
		}

		constexpr Rect_Orthogonal& expand(const T x, const T y) noexcept{
			src.x -= x;
			src.y -= y;

			if constexpr (std::is_unsigned_v<T>){
				size_.x += x * TWO;
				size_.y += y * TWO;
			}else{
				Rect_Orthogonal::addSize(x * TWO, y * TWO);
			}

			return *this;
		}

		/**
		 * @brief
		 * @param marginX Negative is acceptable
		 * @return
		 */
		constexpr Rect_Orthogonal& shrinkX(T marginX) noexcept{
			marginX = Math::min(marginX, size_.x / TWO);
			src.x += marginX;
			size_.x -= marginX * TWO;

			return *this;
		}

		constexpr Rect_Orthogonal& shrinkY(T marginY) noexcept{
			marginY = Math::min(marginY, size_.y / TWO);
			src.y += marginY / TWO;
			size_.y -= marginY * TWO;

			return *this;
		}

		constexpr Rect_Orthogonal& shrink(const T marginX, const T marginY) noexcept{
			(void)this->shrinkX(marginX);
			(void)this->shrinkY(marginY);

			return *this;
		}

		constexpr Rect_Orthogonal& shrink(const typename Geom::Vector2D<T>::PassType margin) noexcept{
			(void)this->shrinkX(margin.x);
			(void)this->shrinkY(margin.y);

			return *this;
		}

		constexpr Rect_Orthogonal& shrink(const T margin) noexcept{
			return this->shrink(margin, margin);
		}

		constexpr Rect_Orthogonal intersectionWith(const Rect_Orthogonal& r) const{
			T maxSrcX = Math::max(getSrcX(), r.getSrcX());
			T maxSrcY = Math::max(getSrcY(), r.getSrcY());

			T minEndX = Math::min(getEndX(), r.getEndX());
			T minEndY = Math::min(getEndY(), r.getEndY());

			// ADAPTED_ASSUME(minEndX >= maxSrcX);
			// ADAPTED_ASSUME(minEndY >= maxSrcY);

			return Rect_Orthogonal{maxSrcX, maxSrcY, Math::clampPositive(minEndX - maxSrcX), Math::clampPositive(minEndY - maxSrcY)};
		}

		[[nodiscard]] constexpr Rect_Orthogonal copy() const noexcept{
			return *this;
		}

		constexpr void each(std::invocable<Vector2D<T>> auto&& func) const requires std::is_integral_v<T>{
			for(T x = src.x; x < getEndX(); ++x){
				for(T y = src.y; y < getEndY(); ++y){
					func(Vector2D<T>{x, y});
				}
			}
		}

		constexpr void each_jumpSrc(std::invocable<Vector2D<T>> auto&& func) const requires std::is_integral_v<T>{
			for(T x = src.x; x < getEndX(); ++x){
				for(T y = src.y; y < getEndY(); ++y){
					if(x != src.x && y != src.y)func(Vector2D<T>{x, y});
				}
			}
		}

		[[deprecated("Unsafe Design")]] struct iterator{
			Vector2D<T> cur{};
			T srcX{};
			T endX{};

			constexpr iterator& operator++() noexcept{
				++cur.x;
				if(cur.x == endX){
					cur.x = srcX;
					++cur.y;
				}

				return *this;
			}

			constexpr iterator operator++(int) noexcept{
				iterator cpy = *this;
				this->operator++();

				return cpy;
			}

			constexpr Vector2D<T> operator*() const noexcept{
				return cur;
			}

			constexpr Vector2D<T> operator->() const noexcept{
				return cur;
			}

			constexpr friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept = default;
		};

		[[deprecated("Unsafe Design")]] constexpr iterator begin() const noexcept{
			return {getSrc(), getSrcX(), getEndX()};
		}

		[[deprecated("Unsafe Design")]] constexpr iterator end() const noexcept{
			return {getEnd(), getEndX(), getEndX()};
		}
	};

	using OrthoRectFloat = Rect_Orthogonal<float>;
	using OrthoRectInt = Rect_Orthogonal<int>;
	using OrthoRectUInt = Rect_Orthogonal<unsigned int>;
}

module;

#include "../src/arc/math/simd.hpp"
#include "../src/ext/adapted_attributes.hpp"

export module Geom.Shape.RectBox;

import Geom.Rect_Orthogonal;
import Geom.Transform;

import Math;
import std;


export namespace Geom {
	/**
	 * \brief Mainly Used For Continous Collidsion Test
	 * @code
	 * v3 +-----+ v2
	 *    |     |
	 *    |     |
	 * v0 +-----+ v1
	 * @endcode
	 */
	struct QuadBox {
		using VertGroup = std::array<Vec2, 4>;

		alignas(8)

		Vec2 v0{};
		Vec2 v1{};
		Vec2 v2{};
		Vec2 v3{};

	protected:
		/**
		 * \brief Exported Vert [bottom-left, bottom-right, top-right, top-left], dynamic calculated
		 */
		OrthoRectFloat maxOrthoBound{};

	public:
		[[nodiscard]] constexpr QuadBox() noexcept = default;

		[[nodiscard]] constexpr QuadBox(const Vec2 v0, const Vec2 v1, const Vec2 v2, const Vec2 v3) noexcept
			: v0{v0},
			  v1{v1},
			  v2{v2},
			  v3{v3}{
			updateBound();
		}

		[[nodiscard]] explicit constexpr QuadBox(const OrthoRectFloat rect) noexcept
			: v0{rect.vert_00()},
			  v1{rect.vert_10()},
			  v2{rect.vert_11()},
			  v3{rect.vert_01()}, maxOrthoBound{rect}{}


		[[nodiscard]] constexpr Vec2 operator[](const int i) const noexcept{
			switch(i) {
				case 0 : return v0;
				case 1 : return v1;
				case 2 : return v2;
				case 3 : return v3;
				default: std::unreachable();
			}
		}

		SIMD_DISABLED_CONSTEXPR void copyAndMove(const Vec2 trans, const QuadBox& other) noexcept{
			this->operator=(other);
			move(trans);
		}

		SIMD_DISABLED_CONSTEXPR void move(const Vec2 vec2) noexcept{
#if SIMD_ENABLED
			// Load vec2 into a SIMD register
			const __m128 vec2Simd = _mm_set_ps(vec2.x, vec2.y, vec2.x, vec2.y);

			// Load v0, v1, v2, and v3 into SIMD registers
			__m128 v0Simd = _mm_loadu_ps(reinterpret_cast<const float*>(&v0));
			__m128 v1Simd = _mm_loadu_ps(reinterpret_cast<const float*>(&v1));
			__m128 v2Simd = _mm_loadu_ps(reinterpret_cast<const float*>(&v2));
			__m128 v3Simd = _mm_loadu_ps(reinterpret_cast<const float*>(&v3));

			// Perform the move operation
			v0Simd = _mm_add_ps(v0Simd, vec2Simd);
			v1Simd = _mm_add_ps(v1Simd, vec2Simd);
			v2Simd = _mm_add_ps(v2Simd, vec2Simd);
			v3Simd = _mm_add_ps(v3Simd, vec2Simd);

			// Store the results back to v0, v1, v2, and v3
			_mm_storeu_ps(reinterpret_cast<float*>(&v0), v0Simd);
			_mm_storeu_ps(reinterpret_cast<float*>(&v1), v1Simd);
			_mm_storeu_ps(reinterpret_cast<float*>(&v2), v2Simd);
			_mm_storeu_ps(reinterpret_cast<float*>(&v3), v3Simd);

#else

			v0 += vec2;
			v1 += vec2;
			v2 += vec2;
			v3 += vec2;
#endif

			maxOrthoBound.src += vec2;
		}

		SIMD_DISABLED_CONSTEXPR void move(const Vec2 vec2, const float scl) noexcept{
			move(vec2 * scl);
		}

		[[nodiscard]] constexpr OrthoRectFloat getMaxOrthoBound() const noexcept{
			return maxOrthoBound;
		}

		constexpr void updateBound() noexcept{
			const auto [xMin, xMax] = Math::minmax(v0.x, v1.x, v2.x, v3.x);
			const auto [yMin, yMax] = Math::minmax(v0.y, v1.y, v2.y, v3.y);

			CHECKED_ASSUME(yMin <= yMax);
			CHECKED_ASSUME(xMin <= xMax);

			maxOrthoBound.src = {xMin, yMin};
			maxOrthoBound.setSize(xMax - xMin, yMax - yMin);
		}


		constexpr friend bool operator==(const QuadBox& lhs, const QuadBox& rhs) noexcept{
			return lhs.v0 == rhs.v0
				&& lhs.v1 == rhs.v1
				&& lhs.v2 == rhs.v2
				&& lhs.v3 == rhs.v3;
		}

		[[nodiscard]] SIMD_DISABLED_CONSTEXPR bool overlapExact(const QuadBox& other,
			const Vec2 axis_1, const Vec2 axis_2,
			const Vec2 axis_3, const Vec2 axis_4
		) const noexcept{
			return
				axisOverlap(other, axis_1) && axisOverlap(other, axis_2) &&
				axisOverlap(other, axis_3) && axisOverlap(other, axis_4);
		}

		[[nodiscard]] constexpr bool overlapRough(const QuadBox& other) const noexcept{
			return
				maxOrthoBound.overlap_Exclusive(other.maxOrthoBound);
		}

		[[nodiscard]] constexpr bool overlapRough(const OrthoRectFloat other) const noexcept{
			return
				maxOrthoBound.overlap_Exclusive(other);
		}

		[[nodiscard]] SIMD_DISABLED_CONSTEXPR bool overlapExact(const OrthoRectFloat other) const noexcept{
			return overlapExact(QuadBox{other},
				Geom::norXVec2<float>,
				Geom::norYVec2<float>,
				getNormalVec(0),
				getNormalVec(1)
			);
		}

		[[nodiscard]] constexpr bool contains(const Vec2 point) const noexcept{
			bool oddNodes = false;

			for(int i = 0; i < 4; ++i){
				const Vec2 vertice     = this->operator[](i);
				const Vec2 lastVertice = this->operator[]((i + 1) % 4);
				if(
					(vertice.y < point.y && lastVertice.y >= point.y) ||
					(lastVertice.y < point.y && vertice.y >= point.y)){

					if(vertice.x + (point.y - vertice.y) * (lastVertice - vertice).slopeInv() < point.x){
						oddNodes = !oddNodes;
					}
				}
			}

			return oddNodes;
		}

		[[nodiscard]] Vec2 getNormalVec(const int edgeIndex) const noexcept{
			const auto begin = this->operator[](edgeIndex);
			const auto end   = this->operator[]((edgeIndex + 1) % 4);

			return (begin - end).normalize().rotateRT();
		}


	protected:
		[[nodiscard]] SIMD_DISABLED_CONSTEXPR bool axisOverlap(const QuadBox& other, const Vec2 axis) const noexcept{
#if SIMD_ENABLED
			const __m128 vertices1_x = _mm_set_ps(v0.x, v1.x, v2.x, v3.x);
			const __m128 vertices1_y = _mm_set_ps(v0.y, v1.y, v2.y, v3.y);
			const __m128 vertices2_x = _mm_set_ps(other.v0.x, other.v1.x, other.v2.x, other.v3.x);
			const __m128 vertices2_y = _mm_set_ps(other.v0.y, other.v1.y, other.v2.y, other.v3.y);

			const __m128 axis_x = _mm_set1_ps(axis.x);
			const __m128 axis_y = _mm_set1_ps(axis.y);

			const __m128 dot1 = _mm_add_ps(_mm_mul_ps(vertices1_x, axis_x), _mm_mul_ps(vertices1_y, axis_y));
			const __m128 dot2 = _mm_add_ps(_mm_mul_ps(vertices2_x, axis_x), _mm_mul_ps(vertices2_y, axis_y));

			std::array<float, 4> dot1_array{};
			std::array<float, 4> dot2_array{};
			_mm_storeu_ps(dot1_array.data(), dot1);
			_mm_storeu_ps(dot2_array.data(), dot2);

			auto [min1, max1] = Math::minmax(dot1_array[0], dot1_array[1], dot1_array[2], dot1_array[3]);
			auto [min2, max2] = Math::minmax(dot2_array[0], dot2_array[1], dot2_array[2], dot2_array[3]);

			return max1 >= min2 && max2 >= min1;
#else
			auto [min1, max1] = Math::minmax(v0.dot(axis), v1.dot(axis), v2.dot(axis), v3.dot(axis));
			auto [min2, max2] = Math::minmax(other.v0.dot(axis), other.v1.dot(axis), other.v2.dot(axis), other.v3.dot(axis));
			return max1 >= min2 && max2 >= min1;
#endif

		}
	};

	struct RectBoxIdentity{
		/**
		 * \brief x for rect width, y for rect height, static
		 */
		Vec2 size;

		/**
		 * \brief Center To Bottom-Left Offset
		 */
		Vec2 offset;


		[[deprecated]] constexpr void setSize(const float w, const float h) noexcept {
			size.set(w, h);
		}
	};

	struct RectBoxBrief : public QuadBox{
		using QuadBox::QuadBox;

	protected:
		/**
		 * \brief
		 * Normal Vector for v0-v1, v2-v3
		 * Edge Vector for v1-v2, v3-v0
		 */
		Vec2 normalU{};

		/**
		 * \brief
		 * Normal Vector for v1-v2, v3-v0
		 * Edge Vector for v0-v1, v2-v3
		 */
		Vec2 normalV{};
	public:
		[[nodiscard]] RectBoxBrief() = default;

		[[nodiscard]] constexpr RectBoxBrief(const Vec2 v0, const Vec2 v1, const Vec2 v2, const Vec2 v3) noexcept
			: QuadBox{v0, v1, v2, v3}{
			updateNormal();
		}

		constexpr void updateNormal() noexcept{
			normalU = (v0 - v3)/*.normalize()*/;
			normalV = (v1 - v2)/*.normalize()*/;
		}

		[[nodiscard]] constexpr Vec2 getNormalU() const noexcept{
			return normalU;
		}

		[[nodiscard]] constexpr Vec2 getNormalV() const noexcept{
			return normalV;
		}

		[[nodiscard]] SIMD_DISABLED_CONSTEXPR bool overlapExact(const RectBoxBrief& other) const {
			return
				axisOverlap(other, normalU) && axisOverlap(other, normalV) &&
				axisOverlap(other, other.normalU) && axisOverlap(other, other.normalV);
		}

		[[nodiscard]] SIMD_DISABLED_CONSTEXPR bool overlapExact(const QuadBox& other, const Vec2 normalU, const Vec2 normalV) const {
			return
				axisOverlap(other, this->normalU) && axisOverlap(other, this->normalV) &&
				axisOverlap(other, normalU) && axisOverlap(other, normalV);
		}

		[[nodiscard]] constexpr float projLen2(const Vec2 axis) const {
			const Vec2 diagonal = v2 - v0;
			const float dot = diagonal.dot(axis);
			return dot * dot / axis.length2();
		}

		[[nodiscard]] float projLen(const Vec2 axis) const {
			return std::sqrt(projLen2(axis));
		}

		[[nodiscard]] constexpr bool contains(const Vec2 point) const {
			bool oddNodes = false;

			for(int i = 0; i < 4; ++i){
				const Vec2 vertice     = this->operator[](i);
				const Vec2 lastVertice = this->operator[]((i + 1) % 4);
				const Vec2 curEdge = i & 1 ? normalU : normalV;
				if((vertice.y < point.y && lastVertice.y >= point.y) || (lastVertice.y < point.y && vertice.y >= point.y)){
					if(vertice.x + (point.y - vertice.y) / curEdge.y * curEdge.x < point.x){
						oddNodes = !oddNodes;
					}
				}
			}
			return oddNodes;
		}

		[[nodiscard]] constexpr Vec2 getNormalVec(const int index) const {
			switch(index) {
				case 0 : return -normalU;
				case 1 : return normalV;
				case 2 : return normalU;
				case 3 : return -normalV;
				default: std::unreachable();
			}
		}
	};

	struct RectBox : RectBoxBrief, RectBoxIdentity{
		/**
		 * \brief Box Origin Point
		 * Should Be Mass Center if possible!
		 */
		Transform transform{};

		using QuadBox::axisOverlap;
		using QuadBox::copyAndMove;
		using QuadBox::overlapRough;
		using QuadBox::overlapExact;
		using RectBoxBrief::overlapExact;

		[[nodiscard]] constexpr RectBox() = default;

		[[nodiscard]] explicit RectBox(const Vec2 size, const Vec2 offset, const Transform transform = {})
			: RectBoxIdentity{size, offset}{
			update(transform);
		}

		constexpr RectBox& copyNecessaryFrom(const RectBox& other) noexcept{
			transform = other.transform;
			size = other.size;
			offset = other.offset;

			return *this;
		}

		/**
		 *	TODO move this to other place?
		 * \param mass
		 * \param scale Manually assign a correction scale
		 * \param lengthRadiusRatio to decide the R(radius) scale for simple calculation
		 * \brief From: [mr^2/4 + ml^2 / 12]
		 * \return Rotational Inertia Estimation
		 */
		[[nodiscard]] constexpr float getRotationalInertia(const float mass, const float scale = 1 / 12.0f, const float lengthRadiusRatio = 0.25f) const noexcept {
			return size.length2() * (scale + lengthRadiusRatio) * mass;
		}

		constexpr void update(const Transform transform) noexcept{
			update(transform.vec, transform.rot);
		}

		constexpr void update(const Vec2 pos, const float rot) noexcept{
			transform.vec = pos;


			const float cos = Math::cosDeg(rot);
			const float sin = Math::sinDeg(rot);

			v0.set(offset).rotate(cos, sin);
			v1.set(size.x, 0).rotate(cos, sin);
			v3.set(0, size.y).rotate(cos, sin);
			v2 = v1 + v3;

			if(transform.rot != rot){
				normalU = v3;
				normalV = v1;
				// normalU.normalize();
				// normalV.normalize();

				transform.rot = rot;
			}


			v0 += transform.vec;
			v1 += v0;
			v2 += v0;
			v3 += v0;

			updateBound();
		}
	};


	//WTF is this shit??
	// RectBoxBrief genRectBoxBrief_byQuad(const OrthoRectFloat src, const Vec2 dir){
	// 	const float ang = dir.angle();
	//
	// 	const float cos = Math::cosDeg(-ang);
	// 	const float sin = Math::sinDeg(-ang);
	//
	// 	std::array verts{
	// 		src.vert_00().rotate(cos, sin),
	// 		src.vert_10().rotate(cos, sin),
	// 		src.vert_11().rotate(cos, sin),
	// 		src.vert_01().rotate(cos, sin)};
	//
	// 	// float minX = std::numeric_limits<float>::max();
	// 	// float minY = std::numeric_limits<float>::max();
	// 	// float maxX = std::numeric_limits<float>::lowest();
	// 	// float maxY = std::numeric_limits<float>::lowest();
	//
	// 	auto [minX, maxX] = minmax(verts[0].x, verts[1].x, verts[2].x, verts[3].x);
	// 	const auto [minY, maxY] = minmax(verts[0].y, verts[1].y, verts[2].y, verts[3].y);
	//
	// 	// for (auto [x, y] : verts){
	// 	// 	minX = Math::min(minX, x);
	// 	// 	minY = Math::min(minY, y);
	// 	// 	maxX = Math::max(maxX, x);
	// 	// 	maxY = Math::max(maxY, y);
	// 	// }
	//
	// 	maxX += dir.length();
	//
	// 	RectBoxBrief box{
	// 		Vec2{minX, minY}.rotate(cos, -sin),
	// 		Vec2{maxX, minY}.rotate(cos, -sin),
	// 		Vec2{maxX, maxY}.rotate(cos, -sin),
	// 		Vec2{minX, maxY}.rotate(cos, -sin)
	// 	};
	//
	// 	box.updateBound();
	// 	box.updateNormal();
	//
	// 	return box;
	// }
}

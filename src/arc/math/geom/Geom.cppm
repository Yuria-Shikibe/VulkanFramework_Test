module ;

#include <cassert>

export module Geom;

import std;
import ext.concepts;
import Math;
import Math.Interpolation;

import Geom.Vector2D;

// import Geom.Shape;
// import Geom.Shape.Circle;
import Geom.Shape.RectBox;
import Geom.Rect_Orthogonal;

import ext.array_stack;

// using namespace Geom::Shape;

export namespace Geom {
	/** Returns a point on the segment nearest to the specified point. */
	[[nodiscard]] constexpr Vec2 nearestSegmentPoint(const Vec2 where, const Vec2 start, const Vec2 end) noexcept{
		const float length2 = start.dst2(end);
		if(length2 == 0) [[unlikely]]{
			 return start;
		}

		const float t = ((where.x - start.x) * (end.x - start.x) + (where.y - start.y) * (end.y - start.y)) / length2;
		if(t <= 0) return start;
		if(t >= 1) return end;
		return {start.x + t * (end.x - start.x), start.y + t * (end.y - start.y)};
	}

	constexpr Vec2 intersectCenterPoint(const QuadBox& subject, const QuadBox& object) {
		const float x0 = Math::max(subject.v0.x, object.v0.x);
		const float y0 = Math::max(subject.v0.y, object.v0.y);
		const float x1 = Math::min(subject.v1.x, object.v1.x);
		const float y1 = Math::min(subject.v1.y, object.v1.y);

		return { (x0 + x1) * 0.5f, (y0 + y1) * 0.5f };
	}

	float dstToLine(Vec2 vec2, const Vec2 pointOnLine, const Vec2 directionVec) {
		if(directionVec.isZero()) return vec2.dst(pointOnLine);
		vec2.sub(pointOnLine);
		const auto dot  = vec2.dot(directionVec);
		const auto porj = dot * dot / directionVec.length2();

		return std::sqrtf(vec2.length2() - porj);
	}

	float dstToLineSeg(const Vec2 vec2, const Vec2 p1, Vec2 p2) {
		p2 -= p1;
		return dstToLine(vec2, p1, p2);
	}

	float dst2ToLine(Vec2 from, const Vec2 pointOnLine, const Vec2 directionVec) {
		if(directionVec.isZero()) return from.dst2(pointOnLine);
		from.sub(pointOnLine);
		const auto dot  = from.dot(directionVec);
		const auto porj = dot * dot / directionVec.length2();

		return from.length2() - porj;
	}

	float dst2ToLineSeg(const Vec2 vec2, const Vec2 vert1, Vec2 vert2) {
		vert2 -= vert1;
		return dstToLine(vec2, vert1, vert2);
	}

	float dstToSegment(const Vec2 p, const Vec2 a, const Vec2 b) noexcept {
		const auto nearest = nearestSegmentPoint(p, a, b);

		return nearest.dst(p);
	}

	constexpr float dst2ToSegment(const Vec2 p, const Vec2 a, const Vec2 b) noexcept {
		const auto nearest = nearestSegmentPoint(p, a, b);

		return nearest.dst2(p);
	}

	[[deprecated]] Vec2 arrive(const Vec2 position, const Vec2 dest, const Vec2 curVel, const float smooth, const float radius, const float tolerance) {
		auto toTarget = Vec2{ dest - position };

		const float distance = toTarget.length();

		if(distance <= tolerance) return toTarget.setZero();
		float targetSpeed = curVel.length();
		if(distance <= radius) targetSpeed *= distance / radius;

		return toTarget.sub(curVel.x / smooth, curVel.y / smooth).limitMax(targetSpeed);
	}

	constexpr Vec2 intersectionLine(const Vec2 p11, const Vec2 p12, const Vec2 p21, const Vec2 p22) {
		const float x1 = p11.x, x2 = p12.x, x3 = p21.x, x4 = p22.x;
		const float y1 = p11.y, y2 = p12.y, y3 = p21.y, y4 = p22.y;

		const float dx1 = x1 - x2;
		const float dy1 = y1 - y2;

		const float dx2 = x3 - x4;
		const float dy2 = y3 - y4;

		const float det = dx1 * dy2 - dy1 * dx2;

		if (det == 0.0f) {
			return Vec2{(x1 + x2) * 0.5f, (y1 + y2) * 0.5f};  // Return the midpoint of overlapping lines
		}

		const float pre = x1 * y2 - y1 * x2, post = x3 * y4 - y3 * x4;
		const float x   = (pre * dx2 - dx1 * post) / det;
		const float y   = (pre * dy2 - dy1 * post) / det;

		return Vec2{x, y};
	}

	// constexpr auto i = intersectionLine({-1, 0}, {1, 0}, {0, -1}, {0, 1});

	 [[deprecated]] std::optional<Vec2> intersectSegments(const Vec2 p1, const Vec2 p2, const Vec2 p3, const Vec2 p4){
		const float x1 = p1.x, y1 = p1.y, x2 = p2.x, y2 = p2.y, x3 = p3.x, y3 = p3.y, x4 = p4.x, y4 = p4.y;

		const float dx1 = x2 - x1;
		const float dy1 = y2 - y1;

		const float dx2 = x4 - x3;
		const float dy2 = y4 - y3;

		const float d = dy2 * dx1 - dx2 * dy1;
		if(d == 0) return std::nullopt;

		const float yd = y1 - y3;
		const float xd = x1 - x3;

		const float ua = (dx2 * yd - dy2 * xd) / d;
		if(ua < 0 || ua > 1) return std::nullopt;

		const float ub = (dx1 * yd - dy1 * xd) / d;
		if(ub < 0 || ub > 1) return std::nullopt;

		return Vec2{x1 + dx1 * ua, y1 + dy1 * ua};
	}

	bool intersectSegments(const Vec2 p1, const Vec2 p2, const Vec2 p3, const Vec2 p4, Vec2& out) noexcept{
		const float x1 = p1.x, y1 = p1.y, x2 = p2.x, y2 = p2.y, x3 = p3.x, y3 = p3.y, x4 = p4.x, y4 = p4.y;

		const float dx1 = x2 - x1;
		const float dy1 = y2 - y1;

		const float dx2 = x4 - x3;
		const float dy2 = y4 - y3;

		const float d = dy2 * dx1 - dx2 * dy1;
		if(d == 0) return false;

		const float yd = y1 - y3;
		const float xd = x1 - x3;
		const float ua = (dx2 * yd - dy2 * xd) / d;
		if(ua < 0 || ua > 1) return false;

		const float ub = (dx1 * yd - dy1 * xd) / d;
		if(ub < 0 || ub > 1) return false;

		out.set(x1 + dx1 * ua, y1 + dy1 * ua);
		return true;
	}

	template <ext::derived<QuadBox> T>
	[[nodiscard]] constexpr Vec2 nearestEdgeNormal(const Vec2 p, const T& rectangle) noexcept {
		float minDistance = std::numeric_limits<float>::max();
		Vec2 closestEdgeNormal{};

		for (int i = 0; i < 4; i++) {
			auto a = rectangle[i];
			auto b = rectangle[(i + 1) % 4];

			const float d = ::Geom::dst2ToSegment(p, a, b);

			if (d < minDistance) {
				minDistance = d;
				closestEdgeNormal = rectangle.getNormalVec(i);
			}
		}

		return closestEdgeNormal;
	}

	template <std::derived_from<QuadBox> T>
	[[nodiscard]] constexpr Vec2 avgEdgeNormal(const Vec2 where, const T& rectangle) noexcept {
		std::array<std::pair<float, Vec2>, 4> normals{};

		for (unsigned i = 0u; i < 4u; i++) {
			const Vec2 va = rectangle[i];
			const Vec2 vb = rectangle[(i + 1u) % 4u];

			normals[i].first = Geom::dstToSegment(where, va, vb) * va.dst(vb);
			normals[i].second = rectangle.getNormalVec(i);
		}

		const float total = (normals[0].first + normals[1].first + normals[2].first + normals[3].first);
		assert(total != 0.f);

		Vec2 closestEdgeNormal{};

		for(const auto& [weight, normal] : normals) {
			closestEdgeNormal.sub(normal * Math::powIntegral<32>(weight / total));
		}

		return closestEdgeNormal;
	}

	struct IntersectionResult{
		struct Info{
			Vec2 pos;
			unsigned short edgeSbj;
			unsigned short edgeObj;

			static bool onSameEdge(const unsigned short edgeIdxN_1, const unsigned short edgeIdxN_2) noexcept {
				return edgeIdxN_1 == edgeIdxN_2;
			}

			static bool onNearEdge(const unsigned short edgeIdxN_1, const unsigned short edgeIdxN_2) noexcept {
				return (edgeIdxN_1 + 1) % 4 == edgeIdxN_2 || (edgeIdxN_2 + 1) % 4 == edgeIdxN_1;
			}

		};
		ext::array_stack<Info, 4, unsigned> points{};

		explicit operator bool() const noexcept{
			return !points.empty();
		}

		[[nodiscard]] constexpr Vec2 avg() const noexcept{
			assert(!points.empty());

			Vec2 rst{};
			for (const auto & intersection : points){
				rst += intersection.pos;
			}

			return rst / static_cast<float>(points.size());
		}
	};

	auto rectExactAvgIntersection(const QuadBox& subjectBox, const QuadBox& objectBox) noexcept {
		IntersectionResult result{};

		Vec2 rst{};
		for(unsigned i = 0; i < 4; ++i) {
			for(unsigned j = 0; j < 4; ++j) {
				if(intersectSegments(
					subjectBox[i], subjectBox[(i + 1) % 4],
					objectBox[j], objectBox[(j + 1) % 4],
					rst)
				) {
					if(!result.points.full()){
						result.points.push(IntersectionResult::Info{rst, static_cast<unsigned short>(i), static_cast<unsigned short>(j)});
					}else{
						return result;
					}
				}
			}
		}

		return result;
	}

	Vec2 rectRoughAvgIntersection(const QuadBox& quad1, const QuadBox& quad2) noexcept {
		Vec2 intersections{};
		unsigned count = 0;

		Vec2 rst{};
		for(int i = 0; i < 4; ++i) {
			for(int j = 0; j < 4; ++j) {
				if(intersectSegments(quad1[i], quad1[(i + 1) % 4], quad2[j], quad2[(j + 1) % 4], rst)) {
					count++;
					intersections += rst;
				}
			}
		}

		if(count > 0) {
			return intersections.div(static_cast<float>(count));
		}else {
			//contains but with no intersection on edge
			return intersections.set(quad1.v0).add(quad1.v2).add(quad2.v0).add(quad2.v2).scl(0.25f);
		}

	}


	OrthoRectFloat maxContinousBoundOf(const std::vector<QuadBox>& traces) {
		const auto& front = traces.front().getMaxOrthoBound();
		const auto& back  = traces.back().getMaxOrthoBound();
		auto [minX, maxX] = std::minmax({ front.getSrcX(), front.getEndX(), back.getSrcX(), back.getEndX() });
		auto [minY, maxY] = std::minmax({ front.getSrcY(), front.getEndY(), back.getSrcY(), back.getEndY() });

		return OrthoRectFloat{ minX, minY, maxX - minX, maxY - minY };
	}

	template <ext::number T>
	bool overlap(const T x, const T y, const T radius, const Rect_Orthogonal<T>& rect) {
		T closestX = std::clamp<T>(x, rect.getSrcX(), rect.getEndX());
		T closestY = std::clamp<T>(y, rect.getSrcY(), rect.getEndY());

		T distanceX       = x - closestX;
		T distanceY       = y - closestY;
		T distanceSquared = distanceX * distanceX + distanceY * distanceY;

		return distanceSquared <= radius * radius;
	}

	// template <ext::number T>
	// bool overlap(const Circle<T>& circle, const Rect_Orthogonal<T>& rect) {
	// 	return Geom::overlap<T>(circle.getCX(), circle.getCY(), circle.getRadius(), rect);
	// }
}

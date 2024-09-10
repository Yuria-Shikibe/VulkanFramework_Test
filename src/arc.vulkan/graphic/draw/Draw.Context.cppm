//
// Created by Matrix on 2024/8/28.
//

export module Graphic.Draw.Func;

export import Graphic.Color;
export import Geom.Vector2D;
export import Geom.Rect_Orthogonal;
export import Geom.Transform;
export import Graphic.ImageRegion;

export import Graphic.Draw.Interface;

import ext.meta_programming;
import std;
import ext.Concepts;
import Math;

export namespace Graphic::Draw{
	constexpr float CircleVertPrecision{8};

	constexpr std::uint32_t getCircleVertices(const float radius){
		return Math::max(Math::ceil(radius * Math::PI / CircleVertPrecision), 12);
	}

	// using Vertex = Core::Vulkan::Vertex_World;
	template <typename Vertex>
	struct Drawer{
		template <VertModifier<Vertex&> M>
		static void fill(
			const DrawParam<M>& param,
			const Geom::Vec2& v00, const Geom::Vec2& v10, const Geom::Vec2& v11, const Geom::Vec2& v01,
			const Color& c00, const Color& c10, const Color& c11, const Color& c01){
			SeqGenerator<Vertex> generator{param.dataPtr};
			generator(DefGenerator<Vertex>::value, param.modifier, v00, param.index, c00, param.uv->v01);
			generator(DefGenerator<Vertex>::value, param.modifier, v10, param.index, c10, param.uv->v11);
			generator(DefGenerator<Vertex>::value, param.modifier, v11, param.index, c11, param.uv->v10);
			generator(DefGenerator<Vertex>::value, param.modifier, v01, param.index, c01, param.uv->v00);
		}

		template <VertModifier<Vertex&> M>
		static void rect(
			const DrawParam<M>& param,
			const Geom::Transform center,
			Geom::Vec2 size,
			const Color& color
		){
			size *= .5f;

			const float sin = Math::sinDeg(center.rot);
			const float cos = Math::cosDeg(center.rot);
			const float w1 = cos * size.x * 0.5f;
			const float h1 = sin * size.x * 0.5f;

			const float w2 = -sin * size.y * 0.5f;
			const float h2 = cos * size.y * 0.5f;
			Drawer::fill(param,
			             center.vec + Geom::Vec2{-w1 - w2, -h1 - h2},
			             center.vec + Geom::Vec2{+w1 - w2, +h1 - h2},
			             center.vec + Geom::Vec2{+w1 + w2, +h1 + h2},
			             center.vec + Geom::Vec2{-w1 + w2, -h1 + h2},
			             color, color, color, color
			);
		}

		template <VertModifier<Vertex&> M>
		static void rectOrtho(
			const DrawParam<M>& param,
			const Geom::OrthoRectFloat& bound,
			const Color& color
		){
			Drawer::fill(param,
			             bound.vert_00(),
			             bound.vert_10(),
			             bound.vert_11(),
			             bound.vert_01(),
			             color, color, color, color
			);
		}

		struct Line{
			//TODO ortho lines
			template <VertModifier<Vertex&> M>
			static void line(
				const DrawParam<M>& param,
				const float stroke,
				const Geom::Vec2 src, const Geom::Vec2 dst,
				const Color& c1, const Color& c2, const bool cap = true){
				const float h_stroke = stroke / 2.0f;
				Geom::Vec2 diff = dst - src;

				const float len = diff.length();
				diff *= h_stroke / len;

				if(cap){
					Drawer::fill(param,
					             src + Geom::Vec2{-diff.x - diff.y, -diff.y + diff.x},
					             src + Geom::Vec2{-diff.x + diff.y, -diff.y - diff.x},
					             dst + Geom::Vec2{+diff.x + diff.y, +diff.y - diff.x},
					             dst + Geom::Vec2{+diff.x - diff.y, +diff.y + diff.x},
					             c1, c1, c2, c2
					);
				} else{
					Drawer::fill(param,
					             src + Geom::Vec2{-diff.y, +diff.x},
					             src + Geom::Vec2{+diff.y, -diff.x},
					             dst + Geom::Vec2{+diff.y, -diff.x},
					             dst + Geom::Vec2{-diff.y, +diff.x},
					             c1, c1, c2, c2
					);
				}
			}

			template <VertModifier<Vertex&> M>
			static void line(
				const DrawParam<M>& param,
				const float stroke,
				const Geom::Vec2 src, const Geom::Vec2 dst,
				const Color& c, const bool cap = true){
				Line::line(param, stroke, src, dst, c, cap);
			}

			template <VertModifier<Vertex&> M>

			static void lineAngleCenter(const DrawParam<M>& param,
			                            const float stroke, const Geom::Transform trans, const float length, const Color& c,
			                            const bool cap = true){
				Geom::Vec2 vec{};

				vec.setPolar(trans.rot, length * 0.5f);

				Line::line(param, stroke, trans.vec + vec, trans.vec - vec, c, c, cap);
			}

			template <VertModifier<Vertex&> M>

			static void lineAngle(const DrawParam<M>& param,
			                      const float stroke, const Geom::Transform trans, const float length, const Color& c,
			                      const bool cap = true){
				Geom::Vec2 vec{};
				vec.setPolar(trans.rot, length);

				Line::line(param, stroke, trans.vec, trans.vec + vec, c, c, cap);
			}


			template <AutoParam M>
			static void rectOrtho(
				M& param,
				const float stroke,
				const Geom::OrthoRectFloat& rect, const Color& color, const bool cap = true){
				Line::line(++param, stroke, rect.vert_00(), rect.vert_01(), color, color, cap);
				Line::line(++param, stroke, rect.vert_01(), rect.vert_11(), color, color, cap);
				Line::line(++param, stroke, rect.vert_11(), rect.vert_10(), color, color, cap);
				Line::line(++param, stroke, rect.vert_10(), rect.vert_00(), color, color, cap);
			}

			template <AutoParam M>
			static void square(
				M& param,
				const float stroke,
				Geom::Transform trans,
				const float radius,
				const Color& color){
				trans.rot += 45.000f;
				const float dst = stroke / Math::SQRT2;

				Geom::Vec2 vec2_0{}, vec2_1{}, vec2_2{}, vec2_3{}, vec2_4{};

				vec2_0.setPolar(trans.rot, 1);

				vec2_1.set(vec2_0);
				vec2_2.set(vec2_0);

				vec2_1.scl(radius - dst);
				vec2_2.scl(radius + dst);

				[&]<std::size_t... I>(std::index_sequence<I...>){
					vec2_0.rotateRT();

					vec2_3.set(vec2_0).scl(radius - dst);
					vec2_4.set(vec2_0).scl(radius + dst);

					Drawer::fill(++param, vec2_1 + trans.vec, vec2_2 + trans.vec, vec2_4 + trans.vec, vec2_3 + trans.vec,
					             color, color, color, color
					);

					vec2_1.set(vec2_3);
					vec2_2.set(vec2_4);
				}(std::make_index_sequence<4>{});
			}

			template <AutoParam M>
			static void poly(
				M& param,
				const float stroke,
				Geom::Transform trans, const std::uint32_t sides, const float radius, const Color& color){
				const float space = Math::DEG_FULL / static_cast<float>(sides);
				const float h_step = stroke / 2.0f / Math::cosDeg(space / 2.0f);
				const float r1 = radius - h_step;
				const float r2 = radius + h_step;

				for(std::uint32_t i = 0; i < sides; i++){
					const float a = space * static_cast<float>(i) + trans.rot;
					const float cos1 = Math::cosDeg(a);
					const float sin1 = Math::sinDeg(a);
					const float cos2 = Math::cosDeg(a + space);
					const float sin2 = Math::sinDeg(a + space);
					Drawer::fill(
						++param,
						Geom::Vec2{trans.vec.x + r1 * cos1, trans.vec.y + r1 * sin1},
						Geom::Vec2{trans.vec.x + r1 * cos2, trans.vec.y + r1 * sin2},
						Geom::Vec2{trans.vec.x + r2 * cos2, trans.vec.y + r2 * sin2},
						Geom::Vec2{trans.vec.x + r2 * cos1, trans.vec.y + r2 * sin1},
						color, color, color, color
					);
				}
			}


			template <AutoParam M, std::ranges::random_access_range Rng = std::array<Color, 1>>
				requires (std::ranges::sized_range<Rng> && std::convertible_to<const Color&, std::ranges::range_value_t<Rng>>)
			static void poly(
				M& param,
				const float stroke,
				Geom::Transform trans,
				const int sides,
				const float radius,
				const float ratio,
				const Rng& colorGroup
			){
#if DEBUG_CHECK
				if(std::ranges::empty(colorGroup)){
					throw std::invalid_argument("Color group is Empty.");
				}
#endif
				const auto fSides = static_cast<float>(sides);

				const float space = Math::DEG_FULL / fSides;
				const float h_step = stroke / 2.0f / Math::cosDeg(space / 2.0f);
				const float r1 = radius - h_step;
				const float r2 = radius + h_step;

				float currentRatio;

				float currentAng = trans.rot;
				float sin1 = Math::sinDeg(currentAng);
				float cos1 = Math::cosDeg(currentAng);
				float sin2, cos2;

				float progress = 0;
				Color lerpColor1 = colorGroup[0];
				Color lerpColor2 = colorGroup[std::ranges::size(colorGroup) - 1];

				for(; progress < fSides * ratio - 1.0f; ++progress){
					// NOLINT(*-flp30-c)
					// NOLINT(cert-flp30-c)
					currentAng = trans.rot + (progress + 1.0f) * space;

					sin2 = Math::sinDeg(currentAng);
					cos2 = Math::cosDeg(currentAng);

					currentRatio = progress / fSides;

					lerpColor2.lerp(currentRatio, colorGroup);

					Drawer::fill(++param,
					             cos1 * r1 + trans.vec.x, sin1 * r1 + trans.vec.y,
					             cos1 * r2 + trans.vec.x, sin1 * r2 + trans.vec.y,
					             cos2 * r2 + trans.vec.x, sin2 * r2 + trans.vec.y,
					             cos2 * r1 + trans.vec.x, sin2 * r1 + trans.vec.y,
					             lerpColor1, lerpColor1, lerpColor2, lerpColor2
					);

					lerpColor1.set(lerpColor2);

					sin1 = sin2;
					cos1 = cos2;
				}

				currentRatio = ratio;
				const float remainRatio = currentRatio * fSides - progress;

				currentAng = trans.rot + (progress + 1.0f) * space;

				sin2 = Math::lerp(sin1, Math::sinDeg(currentAng), remainRatio);
				cos2 = Math::lerp(cos1, Math::cosDeg(currentAng), remainRatio);

				lerpColor2.lerp<true>(progress / fSides, colorGroup);
				lerpColor2.lerp<true>(lerpColor1, 1.0f - remainRatio);

				Drawer::fill(++param,
				             cos1 * r1 + trans.vec.x, sin1 * r1 + trans.vec.y,
				             cos1 * r2 + trans.vec.x, sin1 * r2 + trans.vec.y,
				             cos2 * r2 + trans.vec.x, sin2 * r2 + trans.vec.y,
				             cos2 * r1 + trans.vec.x, sin2 * r1 + trans.vec.y,
				             lerpColor1, lerpColor1, lerpColor2, lerpColor2
				);
			}

			template <AutoParam M>
			static void circle(M& param,
			                   const float stroke,
			                   Geom::Vec2 pos, const float radius, const Color& color){
				Line::poly(param, stroke, {pos, 0}, getCircleVertices(radius), radius, color);
			}
		};
	};

	inline const ImageViewRegion* WhiteRegion{};

	struct DrawContext{
		Color color{};
		const ImageViewRegion* whiteRegion{WhiteRegion};
		float stroke{1.f};
	};
}

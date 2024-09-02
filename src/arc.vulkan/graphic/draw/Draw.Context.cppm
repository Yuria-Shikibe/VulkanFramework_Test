//
// Created by Matrix on 2024/8/28.
//

export module Graphic.Draw.Context;

export import Graphic.Color;
export import Geom.Vector2D;
export import Geom.Rect_Orthogonal;
export import Geom.Transform;
export import Graphic.ImageRegion;

export import Core.Vulkan.Vertex;

import ext.MetaProgramming;
import std;
import ext.Concepts;
import Math;

export namespace Graphic::Draw{

	template <typename M, typename Vertex>
	concept VertModifier = (std::same_as<std::decay_t<M>, std::identity> || std::regular_invocable<Vertex>);

	template <typename T, typename ...Args>
		requires (std::is_default_constructible_v<T> && std::conjunction_v<std::is_trivially_copy_assignable<Args>...>)
	struct VertexGenerator{
		std::tuple<Args T::*...> projections{};

		template <typename ...MptrArgs>
		explicit constexpr VertexGenerator(MptrArgs ...args) noexcept : projections{args...}{}

		template <std::size_t I, typename Arg>
		void set(T& t, const Arg& arg) const{
			t.*std::get<I>(projections) = arg;
		}

		T& gen(void* place, const Args& ...args) const {
			new (place) T{};

			T& t = *static_cast<T*>(place);

			[&]<std::size_t ...I>(std::index_sequence<I...>){
				(this->template set<I>(t, args), ...);
			}(std::index_sequence_for<Args...>());

			return t;
		}

		template <VertModifier<T&> ModifyCallable = std::identity>
		void genWith(void* place, ModifyCallable func, const Args& ...args) const{
			std::invoke(func, this->gen(place, args...));
		}
	};

	template <typename ...Args>
	VertexGenerator(Args...) ->
		VertexGenerator<typename ext::GetMemberPtrInfo<std::tuple_element_t<0, std::tuple<Args...>>>::ClassType, typename ext::GetMemberPtrInfo<Args>::ValueType...>;

	template <typename Vertex>
	struct ModifierBase : std::type_identity<Vertex>{};

	struct DepthCreator : ModifierBase<Core::Vulkan::Vertex_World>{
		float depth{};

		void operator()(type& v) const{
			v.depth = depth;
		}
	};

	static_assert(VertModifier<std::identity, void>);

	template <typename T>
	struct SeqGenerator{
		T* t{};

		[[nodiscard]] SeqGenerator() = default;

		[[nodiscard]] explicit SeqGenerator(void* p) : t{static_cast<T*>(p)} {}

		template <typename ...Args, VertModifier<T&> ModifyCallable = std::identity>
		void push(const VertexGenerator<T, std::decay_t<Args>...>& Generator, ModifyCallable&& modifier, const Args&... args){
			Generator.template genWith<ModifyCallable>(t, modifier, args...);
			++t;
		}
	};

	template <typename T>
	struct DefGenerator{
		static constexpr int value{[]{
			// ReSharper disable once CppStaticAssertFailure
			static_assert(false, "Invalid Vertex Type");
		}()};
	};

	template <>
	struct DefGenerator<Core::Vulkan::Vertex_UI>{
		static constexpr VertexGenerator value{
			&Core::Vulkan::Vertex_UI::position,
			&Core::Vulkan::Vertex_UI::textureParam,
			&Core::Vulkan::Vertex_UI::color,
			&Core::Vulkan::Vertex_UI::texCoord,
		};;
	};

	template <>
	struct DefGenerator<Core::Vulkan::Vertex_World>{
		static constexpr VertexGenerator value{
			&Core::Vulkan::Vertex_World::position,
			&Core::Vulkan::Vertex_World::textureParam,
			&Core::Vulkan::Vertex_World::color,
			&Core::Vulkan::Vertex_World::texCoord,
		};
	};


	//using Vertex = Core::Vulkan::Vertex_World;
	//using Modifier = std::identity;


	template <typename M = std::identity>
	struct DrawParam{
		void* dataPtr{};
		M modifier{};
		Core::Vulkan::TextureIndex index{};
		const UVData* uv{};
	};

	template <typename M>
	concept AutoParam = requires(M& m){
		requires Concepts::SpecDeriveOf<M, DrawParam>;
		{ ++m } -> std::same_as<M&>;
	};


	template <typename Vertex>
	struct Drawer{
		template <VertModifier<Vertex&> M>
		static void draw(
			const DrawParam<M>& param,
			const Geom::Vec2& v00, const Geom::Vec2& v10, const Geom::Vec2& v11, const Geom::Vec2& v01,
			const Color& c00, const Color& c10, const Color& c11, const Color& c01){
			SeqGenerator<Vertex> generator{param.dataPtr};
			generator.push(DefGenerator<Vertex>::value, param.modifier, v00, param.index, c00, param.uv->v01);
			generator.push(DefGenerator<Vertex>::value, param.modifier, v10, param.index, c10, param.uv->v11);
			generator.push(DefGenerator<Vertex>::value, param.modifier, v11, param.index, c11, param.uv->v10);
			generator.push(DefGenerator<Vertex>::value, param.modifier, v01, param.index, c01, param.uv->v00);
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
			Drawer::draw(param,
				center.vec + Geom::Vec2{- w1 - w2, - h1 - h2},
				center.vec + Geom::Vec2{+ w1 - w2, + h1 - h2},
				center.vec + Geom::Vec2{+ w1 + w2, + h1 + h2},
				center.vec + Geom::Vec2{- w1 + w2, - h1 + h2},
				color, color, color, color
			);
		}

		template <VertModifier<Vertex&> M>
		static void rectOrtho(
			const DrawParam<M>& param,
			const Geom::OrthoRectFloat& bound,
			const Color& color
			){
			Drawer::draw(param,
				bound.vert_00(),
				bound.vert_10(),
				bound.vert_11(),
				bound.vert_01(),
				color, color, color, color
			);
		}

		struct Line{
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
					Drawer::draw(param,
						src + Geom::Vec2{- diff.x - diff.y, - diff.y + diff.x},
						src + Geom::Vec2{- diff.x + diff.y, - diff.y - diff.x},
						dst + Geom::Vec2{+ diff.x + diff.y, + diff.y - diff.x},
						dst + Geom::Vec2{+ diff.x - diff.y, + diff.y + diff.x},
						c1, c1, c2, c2
					);
				} else{
					Drawer::draw(param,
						src + Geom::Vec2{- diff.y, + diff.x},
						src + Geom::Vec2{+ diff.y, - diff.x},
						dst + Geom::Vec2{+ diff.y, - diff.x},
						dst + Geom::Vec2{- diff.y, + diff.x},
						c1, c1, c2, c2
					);
				}
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
		};
	};

	inline const ImageViewRegion* WhiteRegion{};

	struct DrawContext{
		Color color{};
		const ImageViewRegion* whiteRegion{WhiteRegion};
		float stroke{1.f};
	};
}
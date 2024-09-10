export module Graphic.Draw.Interface;

import std;
import ext.meta_programming;
import ext.Concepts;
export import Core.Vulkan.Vertex;
export import Graphic.ImageRegion;

export namespace Graphic::Draw{
	template <typename M, typename Vertex>
	concept VertModifier = (std::same_as<std::decay_t<M>, std::identity> || std::regular_invocable<Vertex>);

	template <typename T, typename... Args>
		requires (std::is_default_constructible_v<T> && std::conjunction_v<std::is_trivially_copy_assignable<Args>...>)
	struct VertexGenerator{
		std::tuple<Args T::*...> projections{};

		template <typename... MptrArgs>
		explicit constexpr VertexGenerator(MptrArgs... args) noexcept : projections{args...}{}

		template <std::size_t I, typename Arg>
		void set(T& t, const Arg& arg) const{
			t.*std::get<I>(projections) = arg;
		}

		T& operator()(void* place, const Args&... args) const{
			new(place) T{};

			T& t = *static_cast<T*>(place);

			[&]<std::size_t ... I>(std::index_sequence<I...>){
				(this->template set<I>(t, args), ...);
			}(std::index_sequence_for<Args...>());

			return t;
		}

		template <VertModifier<T&> ModifyCallable = std::identity>
		void operator()(void* place, ModifyCallable func, const Args&... args) const{
			std::invoke(func, this->operator()(place, args...));
		}
	};

	template <typename... Args>
	VertexGenerator(Args...) ->
		VertexGenerator<
			typename ext::mptr_info<std::tuple_element_t<0, std::tuple<Args...>>>::class_type,
			typename ext::mptr_info<Args>::value_type...>;

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

		[[nodiscard]] explicit SeqGenerator(void* p) : t{static_cast<T*>(p)}{}

		template <typename... Args, VertModifier<T&> ModifyCallable = std::identity>
		void operator()(const VertexGenerator<T, std::decay_t<Args>...>& Generator, ModifyCallable&& modifier,
		                const Args&... args){
			Generator.template operator()<ModifyCallable>(t, modifier, args...);
			++t;
		}
	};

	template <typename T>
	struct DefGenerator{
		static constexpr int value{
				[]{
					// ReSharper disable once CppStaticAssertFailure
					static_assert(false, "Invalid Vertex Type");
				}()
			};
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

	template <typename M = std::identity>
	struct DrawParam{
		void* dataPtr{};
		M modifier{};
		Core::Vulkan::TextureIndex index{};
		const Graphic::UVData* uv{};
	};

	template <typename M>
	concept AutoParam = requires(M& m){
		requires ext::SpecDeriveOf<M, DrawParam>;
		{ ++m } -> std::same_as<M&>;
	};
}

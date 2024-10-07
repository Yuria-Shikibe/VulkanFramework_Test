module;

#include <vulkan/vulkan.h>

#include "../src/ext/no_unique_address.hpp"

export module Graphic.Draw.Interface;

import std;
import ext.meta_programming;
import ext.concepts;
export import Core.Vulkan.Vertex;
export import Graphic.ImageRegion;

export namespace Graphic::Draw{
	template <typename M, typename Vertex>
	concept VertexModifier = std::regular_invocable<M, Vertex>;

	template <typename T, typename... Args>
		requires (std::is_default_constructible_v<T> && (std::is_trivially_copy_assignable_v<Args> && ...))
	struct VertexProjection{
	private:
		using projectionArgs = std::tuple<Args T::*...>;

		template <std::size_t I>
		using argAt = typename ext::mptr_info<std::tuple_element_t<I, projectionArgs>>::value_type;

		projectionArgs projections{};


		template <std::size_t I, typename Arg>
		constexpr void set(T& t, const Arg& arg) const noexcept(std::is_nothrow_assignable_v<argAt<I>, Arg>){
			static_assert(std::is_lvalue_reference_v<std::invoke_result_t<std::tuple_element_t<I, projectionArgs>, T&>>, "must assign to a valid lvalue");
			std::invoke(std::get<I>(projections), t) = arg;
		}

	public:
		template <typename... MptrArgs>
		explicit constexpr VertexProjection(const MptrArgs&... args) noexcept : projections{args...}{}

		constexpr T& operator()(void* place, const Args&... args) const noexcept(std::is_nothrow_constructible_v<T, const Args& ...>){
			T& t = *new(place) T{};

			[&]<std::size_t ... I>(std::index_sequence<I...>){
				(this->template set<I>(t, args), ...);
			}(std::index_sequence_for<Args...>());

			return t;
		}

		template <VertexModifier<T&> ModifyCallable = std::identity>
		constexpr void operator()(void* place, ModifyCallable func, const Args&... args) const noexcept(std::is_nothrow_constructible_v<T, const Args& ...>){
			std::invoke(func, this->operator()(place, args...));
		}
	};

	template <typename... Args>
	VertexProjection(Args...) ->
		VertexProjection<
			typename ext::mptr_info<std::tuple_element_t<0, std::tuple<Args...>>>::class_type,
			typename ext::mptr_info<Args>::value_type...>;

	template <typename Vertex>
	struct ModifierBase : std::type_identity<Vertex>{};

	struct DepthCreator : ModifierBase<Core::Vulkan::Vertex_World>{
		float depth{};

		constexpr void operator()(type& v) const noexcept{
			v.depth = depth;
		}
	};


	template <typename T>
	struct SeqGenerator{
		T* t{};

		[[nodiscard]] SeqGenerator() = default;

		[[nodiscard]] explicit SeqGenerator(void* p) : t{static_cast<T*>(p)}{}

		template <typename... Args, VertexModifier<T&> ModifyCallable = std::identity>
		constexpr void operator()(const VertexProjection<T, std::decay_t<Args>...>& Generator, ModifyCallable modifier,
		                const Args&... args){
			Generator(t, modifier, args...);
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
		static constexpr VertexProjection value{
				&Core::Vulkan::Vertex_UI::position,
				&Core::Vulkan::Vertex_UI::textureParam,
				&Core::Vulkan::Vertex_UI::color,
				&Core::Vulkan::Vertex_UI::texCoord,
			};
	};

	template <>
	struct DefGenerator<Core::Vulkan::Vertex_World>{
		static constexpr VertexProjection value{
				&Core::Vulkan::Vertex_World::position,
				&Core::Vulkan::Vertex_World::textureParam,
				&Core::Vulkan::Vertex_World::color,
				&Core::Vulkan::Vertex_World::texCoord,
			};
	};

	template <typename M = std::identity>
	struct DrawParam{
		void* dataPtr{};
		Core::Vulkan::TextureIndex index{};
		ADAPTED_NO_UNIQUE_ADDRESS M modifier{};

		const UVData* uv{}; //TODO using value instead of indirect value?
	};

	template <typename M>
	concept AutoAcquirableParam = requires(M& m){
		typename M::VertexType;
		requires ext::spec_of<M, DrawParam>;

		{ ++m } -> std::same_as<M&>;
	};

	template <typename M>
	concept ImageMutableParam = requires(M& m){
		{ m << VkImageView{} } -> std::same_as<M&>;
		{ m << std::declval<const UVData&>() } -> std::same_as<M&>;
	};

	template <typename Vty, typename M>
	struct AutoParamBase : DrawParam<M>{
		using VertexType = Vty;

		AutoParamBase& operator ++() = delete;

		AutoParamBase& operator << (VkImageView) = delete;
		AutoParamBase& operator << (const UVData&) = delete;
		AutoParamBase& operator << (const UVData*) = delete;
		AutoParamBase& operator << (const ImageViewRegion&) = delete; //OPTM using CRTP ?
		AutoParamBase& operator << (const ImageViewRegion*) = delete; //OPTM using CRTP ?

	};

	using EmptyParam = AutoParamBase<Core::Vulkan::Vertex_UI, std::identity>;

	template <typename T>
	struct AutoParamTraits{
		using VertexType = typename T::VertexType;
	};
}

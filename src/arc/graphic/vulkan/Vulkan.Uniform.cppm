module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Uniform;

export import Geom.Matrix3D;
import std;

export namespace Core::Vulkan{
	template <typename T, typename ...Ty>
	struct Combinable : std::bool_constant<requires{
		requires std::conjunction_v<std::is_same<T, Ty>...> ||
		(std::conjunction_v<std::is_scalar<Ty>...> && std::is_scalar_v<T> && ((sizeof(Ty) == 4) && ... && (sizeof(T) == 4)));
	}>{};

	template <std::size_t size>
	struct UniformPadding{std::array<std::byte, size> padding{};};

	template <>
	struct UniformPadding<0>{};

	consteval std::size_t nextMultiplesOf(const std::size_t cur, const std::size_t base){
		if (const auto dst = cur % base){
			return cur + (base - dst);
		} else{
			return cur;
		}
	}

	template <typename... T>
		requires (std::conjunction_v<std::is_trivially_copy_assignable<T>...> && Combinable<T...>::value)
	struct UniformValuePadder : UniformPadding<nextMultiplesOf(sizeof(std::tuple<T...>), 16) - sizeof(std::tuple<T...>)>{
		using value_type = std::tuple<T...>;
		value_type value{};

		[[nodiscard]] constexpr UniformValuePadder() = default;

		[[nodiscard]] constexpr UniformValuePadder(const T&... t) noexcept : value{t...} {}

		UniformValuePadder& operator=(const std::tuple_element_t<0, value_type>& other) requires (sizeof...(T) == 1){
			value = other;
			return *this;
		}
	};//TODO support structured binding


	struct UniformMatrix3D{
		std::array<std::array<float, 4>, 3> matrix{};

		[[nodiscard]] constexpr UniformMatrix3D() = default;

		[[nodiscard]] constexpr UniformMatrix3D(const Geom::Matrix3D& mat){
			matrix[0][0] = mat[0 * 3 + 0];
			matrix[0][1] = mat[0 * 3 + 1];
			matrix[0][2] = mat[0 * 3 + 2];

			matrix[1][0] = mat[1 * 3 + 0];
			matrix[1][1] = mat[1 * 3 + 1];
			matrix[1][2] = mat[1 * 3 + 2];

			matrix[2][0] = mat[2 * 3 + 0];
			matrix[2][1] = mat[2 * 3 + 1];
			matrix[2][2] = mat[2 * 3 + 2];
		}

	};

	struct UniformProjectionBlock {
		UniformMatrix3D mat3{};
		float v{};
	};
}
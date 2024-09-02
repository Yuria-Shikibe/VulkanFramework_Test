module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Dependency;
import std;

export namespace Core::Vulkan{
	template <typename T, bool enableCopy = false>
		requires std::is_pointer_v<T>
	struct Wrapper{
	protected:
		T handle{};

	public:
		using WrappedType = T;

		[[nodiscard]] Wrapper() = default;

		[[nodiscard]] explicit Wrapper(const T handler)
			: handle{handler}{}

		T release(){
			T result = handle;
			handle = nullptr;
			return result;
		}

		Wrapper(const Wrapper& other) requires (enableCopy) = default;

		Wrapper& operator=(const Wrapper& other) requires (enableCopy) = default;

		Wrapper(Wrapper&& other) noexcept
			: handle{std::move(other.handle)}{
			other.handle = nullptr;
		}

		Wrapper& operator=(Wrapper&& other) noexcept{
			if(this == &other) return *this;
			std::swap(handle, other.handle);
			return *this;
		}


		[[nodiscard]] constexpr operator T() const noexcept{ return handle; }

		[[nodiscard]] constexpr operator bool() const noexcept{ return handle != nullptr; }

		[[nodiscard]] constexpr const T* operator->() const noexcept{ return &handle; }

		[[nodiscard]] constexpr T* operator->() noexcept{ return &handle; }

		[[nodiscard]] constexpr T get() const noexcept{ return handle; }

		[[nodiscard]] constexpr std::array<T, 1> asSeq() const noexcept{ return std::array{handle}; }

		[[nodiscard]] constexpr const T* asData() const noexcept{ return &handle; }
	};
	/**
	 * @brief Make move constructor default declarable
	 * @tparam T Dependency Type
	 */
	template <typename T>
		requires std::is_pointer_v<T>
	struct Dependency{
		T handle{};

		[[nodiscard]] constexpr Dependency() noexcept = default;

		[[nodiscard]] constexpr Dependency(T device) noexcept : handle{device}{}

		constexpr ~Dependency() = default;

		[[nodiscard]] constexpr operator T() const noexcept{ return handle; }

		[[nodiscard]] constexpr operator bool() const noexcept{ return handle != nullptr; }
		//
		[[nodiscard]] constexpr T operator->() const noexcept{ return handle; }

		[[nodiscard]] constexpr const T* asData() const noexcept{ return &handle; }

		Dependency(const Dependency& other) = delete;

		Dependency(Dependency&& other) noexcept
			: handle{other.handle}{
			other.handle = nullptr;
		}

		constexpr Dependency& operator=(const Dependency& other) = delete;

		constexpr Dependency& operator=(Dependency&& other) noexcept{
			if(this == &other) return *this;
			std::swap(handle, other.handle);
			return *this;
		}
	};

	using DeviceDependency = Dependency<VkDevice>;
}

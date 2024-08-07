module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.LogicalDevice.Dependency;
import std;

export namespace Core::Vulkan{
	/**
	 * @brief Make move constructor default declarable
	 * @tparam T Dependency Type
	 */
	template <typename T>
		requires std::is_pointer_v<T>
	struct Dependency{
		T handler{};

		[[nodiscard]] constexpr Dependency() noexcept = default;

		[[nodiscard]] constexpr Dependency(T device) noexcept : handler{device}{}

		constexpr ~Dependency() = default;

		[[nodiscard]] constexpr operator T() const noexcept{ return handler; }

		[[nodiscard]] constexpr const T* operator->() const noexcept{ return &handler; }

		[[nodiscard]] constexpr T* operator->() noexcept{ return &handler; }

		Dependency(const Dependency& other) = delete;

		Dependency(Dependency&& other) noexcept
			: handler{other.handler}{
			other.handler = nullptr;
		}

		constexpr Dependency& operator=(const Dependency& other) = delete;

		constexpr Dependency& operator=(Dependency&& other) noexcept{
			if(this == &other) return *this;
			handler = other.handler;
			other.handler = nullptr;
			return *this;
		}
	};

	using DeviceDependency = Dependency<VkDevice>;
}

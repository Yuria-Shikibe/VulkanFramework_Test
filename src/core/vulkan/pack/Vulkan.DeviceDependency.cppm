module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.LogicalDevice.Dependency;
import std;

export namespace Core::Vulkan{
	/**
	 * @brief Make move constructor/assignment default declerable
	 * @tparam T Dependency Type
	 */
	template <typename T>
		requires std::is_pointer_v<T>
	struct Dependency{
		T handler{};

		[[nodiscard]] constexpr Dependency() = default;

		[[nodiscard]] constexpr Dependency(T device)
			: handler{device}{}

		~Dependency() = default;

		[[nodiscard]] constexpr operator T() const noexcept{ return handler; }

		Dependency(const Dependency& other) = delete;

		Dependency(Dependency&& other) noexcept
			: handler{other.handler}{
			other.handler = nullptr;
		}

		Dependency& operator=(const Dependency& other) = delete;

		Dependency& operator=(Dependency&& other) noexcept{
			if(this == &other) return *this;
			handler = other.handler;
			other.handler = nullptr;
			return *this;
		}
	};

	using DeviceDependency = Dependency<VkDevice>;
}

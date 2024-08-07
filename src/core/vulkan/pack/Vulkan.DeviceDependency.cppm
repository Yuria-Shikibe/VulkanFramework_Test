module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.LogicalDevice.Dependency;

export namespace Core::Vulkan{
	struct DeviceDependency{
		VkDevice handler{};

		[[nodiscard]] constexpr DeviceDependency() = default;

		[[nodiscard]] constexpr DeviceDependency(VkDevice device)
			: handler{device}{}

		~DeviceDependency() = default;

		[[nodiscard]] constexpr operator VkDevice() const noexcept{ return handler; }

		DeviceDependency(const DeviceDependency& other) = delete;

		DeviceDependency(DeviceDependency&& other) noexcept
			: handler{other.handler}{
			other.handler = nullptr;
		}

		DeviceDependency& operator=(const DeviceDependency& other) = delete;

		DeviceDependency& operator=(DeviceDependency&& other) noexcept{
			if(this == &other) return *this;
			handler = other.handler;
			other.handler = nullptr;
			return *this;
		}
	};
}

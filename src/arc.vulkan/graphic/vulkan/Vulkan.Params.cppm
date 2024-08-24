module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Params;

export namespace Core::Vulkan::Param {
    struct Device {
        VkDevice device;
    };

    struct Device_WithPhysical {
        VkDevice device;
        VkPhysicalDevice physicalDevice;
    };
}
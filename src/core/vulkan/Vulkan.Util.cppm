module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Util;

export import Core.Vulkan.Util.Invoker;
import std;


namespace Core::Vulkan::Util{
	export
	std::vector<VkExtensionProperties> getValidExtensions(){
		auto [extensions, rst] = enumerate(vkEnumerateInstanceExtensionProperties, nullptr);

		return extensions;
	}
}

module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Shader;

import std;

export namespace Core::Vulkan{
	std::vector<char> readFile(const std::filesystem::path& path){
		std::ifstream file(path, std::ios::ate | std::ios::binary);

		if(!file.is_open()){
			throw std::runtime_error("failed to open file!");
		}

		const std::streamoff fileSize = file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);

		return buffer;
	}

	VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code){
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

		VkShaderModule shaderModule;
		if(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS){
			throw std::runtime_error("Failed to create shader module!");
		}

		// if constexpr (DEBUG_CHECK){
		// 	std::println("Generated Shader Module Form")
		// }

		return shaderModule;
	}

	VkShaderModule createShaderModule(VkDevice device, const std::filesystem::path& path){
		return createShaderModule(device, readFile(path));
	}

	struct ShaderModule{
		VkDevice device{};
		VkShaderModule module{};

		[[nodiscard]] explicit ShaderModule(VkDevice device) : device(device){
		}

		[[nodiscard]] explicit ShaderModule(VkDevice device, const std::vector<char>& code) : device(device),
			module(createShaderModule(device, code)){
		}

		[[nodiscard]] explicit ShaderModule(VkDevice device, const std::filesystem::path& path) : device(device),
			module(createShaderModule(device, path)){
		}

		[[nodiscard]] ShaderModule() = default;

		~ShaderModule(){
			if(device && module)vkDestroyShaderModule(device, module, nullptr);
		}

		ShaderModule(const ShaderModule& other) = delete;
		ShaderModule& operator=(const ShaderModule& other) = delete;

		//TODO move impl
		ShaderModule(ShaderModule&& other) noexcept = delete;
		ShaderModule& operator=(ShaderModule&& other) noexcept = delete;

		operator VkShaderModule() const noexcept{
			return module;
		}
	};
}

module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Shader;

import std;
import Core.Vulkan.LogicalDevice.Dependency;
import OS.File;

export namespace Core::Vulkan{
	struct ShaderModule{
		DeviceDependency device{};
		VkShaderModule module{};
		VkShaderStageFlagBits declStage{};

		static constexpr std::array ShaderType{
			std::pair{std::string_view{".vert"}, VK_SHADER_STAGE_VERTEX_BIT},
			std::pair{std::string_view{".frag"}, VK_SHADER_STAGE_FRAGMENT_BIT},
			std::pair{std::string_view{".geom"}, VK_SHADER_STAGE_GEOMETRY_BIT},
			std::pair{std::string_view{".tesc"}, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT},
			std::pair{std::string_view{".tese"}, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT},
			std::pair{std::string_view{".comp"}, VK_SHADER_STAGE_COMPUTE_BIT},
		};

		[[nodiscard]] explicit ShaderModule(VkDevice device) : device(device){
		}

		[[nodiscard]] explicit ShaderModule(VkDevice device, const std::vector<char>& code) : device(device){
			createShaderModule(code);
		}

		[[nodiscard]] explicit ShaderModule(VkDevice device, const OS::File& path) : device(device){
			createShaderModule(path);
			declShaderStage(path);
		}

		[[nodiscard]] ShaderModule() = default;

		VkPipelineShaderStageCreateInfo createInfo(
			VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM,
			const VkSpecializationInfo* specializationInfo = nullptr,
			const std::string_view entry = "main") const{

			if(declStage){
				if(stage == VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM){
					stage = declStage;
				}else if(declStage != stage){
					std::println(std::cerr, "Shader State Mismatch");
				}
			}else{
				if(stage == VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM){
					throw std::runtime_error("Shader Stage must be explicit specified!");
				}
			}

			const VkPipelineShaderStageCreateInfo vertShaderStageInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.stage = stage,
				.module = module,
				.pName = entry.data(),
				.pSpecializationInfo = specializationInfo
			};

			return vertShaderStageInfo;
		}

		void load(VkDevice device, const OS::File& path){
			if(module && this->device)vkDestroyShaderModule(device, module, nullptr);

			this->device = device;
			createShaderModule(path);
			declShaderStage(path);
		}

		~ShaderModule(){
			if(module && device)vkDestroyShaderModule(device, module, nullptr);
		}

		ShaderModule(const ShaderModule& other) = delete;

		ShaderModule(ShaderModule&& other) noexcept = default;

		ShaderModule& operator=(const ShaderModule& other) = delete;

		ShaderModule& operator=(ShaderModule&& other) noexcept = default;

	private:
		void declShaderStage(const OS::File& path){
			const auto fileName = path.filename();

			for (const auto & [type, stage] : ShaderType){
				if(fileName.contains(type)){
					declStage = stage;
					break;
				}
			}
		}

		template <std::ranges::contiguous_range T>
		void createShaderModule(const T& code){
			VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
			createInfo.codeSize = std::ranges::size(code);
			createInfo.pCode = reinterpret_cast<const std::uint32_t*>(std::ranges::data(code));

			if(vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS){
				throw std::runtime_error("Failed to create shader module!");
			}
		}

		void createShaderModule(const OS::File& file){
			createShaderModule(file.readString());
		}
	};

	struct ShaderChain{
		std::vector<VkPipelineShaderStageCreateInfo> chain{};

		void push(const std::initializer_list<ShaderModule> args){
			chain.reserve(args.size());

			for (const auto & arg : args){
				chain.push_back(arg.createInfo());
			}
		}

		void push(const std::initializer_list<VkPipelineShaderStageCreateInfo> args){
			chain = args;
		}

		[[nodiscard]] ShaderChain() = default;

		[[nodiscard]] ShaderChain(const std::initializer_list<ShaderModule> args){
			push(args);
		}

		[[nodiscard]] ShaderChain(const std::initializer_list<VkPipelineShaderStageCreateInfo> args) : chain{args}{}


		[[nodiscard]] VkPipelineShaderStageCreateInfo* data() noexcept{
			return chain.data();
		}

		[[nodiscard]] std::uint32_t size() const noexcept{
			return chain.size();
		}
	};
}

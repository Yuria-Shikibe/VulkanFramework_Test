module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Shader;

import Core.Vulkan.Dependency;
import Core.File;
import std;

export namespace Core::Vulkan{
	struct ShaderModule{
		DeviceDependency device{};
		VkShaderModule shaderModule{};
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

		[[nodiscard]] explicit ShaderModule(VkDevice device, const Core::File& path) : device(device){
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
				.module = shaderModule,
				.pName = entry.data(),
				.pSpecializationInfo = specializationInfo
			};

			return vertShaderStageInfo;
		}

		void load(VkDevice device, const Core::File& path){
			if(shaderModule && this->device)vkDestroyShaderModule(device, shaderModule, nullptr);

			this->device = device;
			createShaderModule(path);
			declShaderStage(path);
		}

		~ShaderModule(){
			if(shaderModule && device)vkDestroyShaderModule(device, shaderModule, nullptr);
		}

		ShaderModule(const ShaderModule& other) = delete;

		ShaderModule(ShaderModule&& other) noexcept = default;

		ShaderModule& operator=(const ShaderModule& other) = delete;

		ShaderModule& operator=(ShaderModule&& other) noexcept{
			if(this == &other) return *this;
			if(shaderModule && device)vkDestroyShaderModule(device, shaderModule, nullptr);
			device = std::move(other.device);
			shaderModule = other.shaderModule;
			declStage = other.declStage;
			return *this;
		}

	private:
		void declShaderStage(const Core::File& path){
			const auto fileName = path.filename();

			for (const auto & [type, stage] : ShaderType){
				if(fileName.contains(type)){
					declStage = stage;
					break;
				}
			}
		}

		template <std::ranges::contiguous_range Rng>
		void createShaderModule(const Rng& code){
			VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
			createInfo.codeSize = std::ranges::size(code) * sizeof(std::ranges::range_value_t<Rng>);
			createInfo.pCode = reinterpret_cast<const std::uint32_t*>(std::ranges::data(code));

			if(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS){
				throw std::runtime_error("Failed to create shader module!");
			}
		}

		void createShaderModule(const Core::File& file){
			createShaderModule(file.readBytes<std::uint32_t>());
		}
	};

	struct ShaderChain{
		std::vector<VkPipelineShaderStageCreateInfo> chain{};

		void push(const std::initializer_list<const ShaderModule*> args){
			chain.reserve(chain.size() + args.size());

			for (const auto & arg : args){
				chain.push_back(arg->createInfo());
			}
		}

		void push(const std::initializer_list<VkPipelineShaderStageCreateInfo> args){
			chain = args;
		}

		[[nodiscard]] ShaderChain() = default;

		[[nodiscard]] explicit(false) ShaderChain(const std::initializer_list<const ShaderModule*> args){
			push(args);
		}

		[[nodiscard]] explicit(false) ShaderChain(const std::initializer_list<VkPipelineShaderStageCreateInfo> args) : chain{args}{}


		[[nodiscard]] VkPipelineShaderStageCreateInfo* data() noexcept{
			return chain.data();
		}

		[[nodiscard]] std::uint32_t size() const noexcept{
			return chain.size();
		}
	};
}

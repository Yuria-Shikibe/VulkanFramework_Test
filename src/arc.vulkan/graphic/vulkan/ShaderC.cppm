module;

#include <shaderc/shaderc.hpp>

export module Core.Vulkan.Shader.Compile;
import std;
import Core.File;

export namespace Core::Vulkan {
    class ShaderRuntimeCompiler {
        static constexpr std::array ValidStages{
            std::string_view{"vertex"},
            std::string_view{"fragment"},
            std::string_view{"compute"},
        };

        struct Includer : public shaderc::CompileOptions::IncluderInterface {

            struct result_t : shaderc_include_result {
                std::string filepath;
                std::string code;
            };

            shaderc_include_result *GetInclude(const char *requested_source, shaderc_include_type type,
                                               const char *requesting_source, size_t include_depth) override {
                auto &result = *(new result_t);
                auto &filepath = result.filepath;
                auto &code = result.code;

                filepath = requesting_source;
                Core::File file{filepath};

                file = file.getParent().find(requested_source);


                filepath = file.getPath().string();

                // OS::File file{filepath};
                code = file.readString();

                static_cast<shaderc_include_result &>(result) = {
                        filepath.c_str(),
                        filepath.length(),
                        code.data(),
                        code.size(),
                        this
                    };

                return &result;
            }

            void ReleaseInclude(shaderc_include_result *data) override {
                delete static_cast<result_t *>(data); // NOLINT(*-pro-type-static-cast-downcast)
            }
        };
        //--------------------
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

    public:
        ShaderRuntimeCompiler() {
            options.SetSourceLanguage(shaderc_source_language_glsl);
            options.SetWarningsAsErrors();
            options.SetGenerateDebugInfo();
            options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
            options.SetTargetSpirv(shaderc_spirv_version_1_6);
            options.SetOptimizationLevel(shaderc_optimization_level_performance);

            options.SetIncluder(std::make_unique<Includer>());
        }
        //C:\VulkanSDK\1.3.290.0\Bin\spirv-val.exe D:\projects\vulkan_framework\properties\shader\spv\fxaa.frag.spv
        auto compile(const std::span<const char> code, const char *filepath, const char *entry = "main") const {
            const shaderc::SpvCompilationResult result =
                compiler.CompileGlslToSpv(
                    code.data(), code.size(),
                    shaderc_glsl_infer_from_source,
                    filepath, entry, options);

            auto err = result.GetErrorMessage();
            if(!err.empty())std::println(std::cerr, "{}", result.GetErrorMessage());

            std::vector<std::uint32_t> bin{result.begin(), result.end()};

            return bin;
        }

        decltype(auto) compile(const Core::File& file, const char* entry = "main") const{
            const auto pStr = file.absolutePath().string();
            const auto code = file.readString();

#ifdef DEBUG_CHECK
            const std::regex pragma_regex(R"(#pragma\s+shader_stage\((\w+)\))");
            std::smatch matches{};

            if(std::regex_search(code.cbegin(), code.cend(), matches, pragma_regex)){
                if(std::ranges::none_of(ValidStages, [str = matches[1].str()](const auto& stage){
                    return stage == str;
                })){
                    throw std::runtime_error("Invalid Shader Stage");
                }
            } else{
                throw std::runtime_error("Ambiguous Shader Stage");
            }

#endif

            return compile(code, pStr.c_str(), entry);
        }

    };

    struct ShaderCompilerWriter {
        const ShaderRuntimeCompiler& compiler;
        Core::File outputDirectory{};

        [[nodiscard]] ShaderCompilerWriter(const ShaderRuntimeCompiler& compiler, const Core::File& outputDirectory) :
            compiler{compiler},
            outputDirectory{outputDirectory} {
            if(!outputDirectory.isDir()) {
                throw std::invalid_argument("Invalid output directory");
            }
        }

        void compile(const Core::File& file) const {
            const auto rst = compiler.compile(file);

            const auto target = outputDirectory.subFile(file.filename() + ".spv");
            target.writeByte(rst);
        }
    };
}

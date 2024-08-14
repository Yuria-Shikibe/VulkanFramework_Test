module;

#include <Windows.h>

export module Core.Vulkan.Shader.Compile;

import std;

export namespace Core::Vulkan{
	const std::filesystem::path DefaultCompilerPath{R"(D:\projects\vulkan_framework\properties\shader\glslangValidator.exe)"};
	const std::filesystem::path TargetCompilerPath{R"(D:\projects\vulkan_framework\properties\shader\spv)"};
	const std::filesystem::path DefaultSrcPath{R"(D:\projects\vulkan_framework\properties\shader\src)"};

	struct ShaderCompileTask{
		std::filesystem::path compilerPath{};
		std::filesystem::path sourcePath{};
		std::filesystem::path outputDirectory{};
		std::string overrideName{};
		std::string appendCommand{};

		[[nodiscard]] ShaderCompileTask() = default;

		[[nodiscard]] ShaderCompileTask(const std::filesystem::path& compilerPath,
		                                const std::filesystem::path& sourcePath,
		                                const std::filesystem::path& outputDirectory = {},
		                                const std::string_view overrideName = {},
		                                const std::string_view appendCommand = {})
			: compilerPath(compilerPath),
			  sourcePath(sourcePath),
			  outputDirectory(outputDirectory),
			  overrideName(overrideName),
			  appendCommand(appendCommand){
		}

	private:
		bool done{false};
		std::filesystem::path targetPath{};

	public:
		[[nodiscard]] const std::filesystem::path& getTargetPath() const noexcept{
			return targetPath;
		}

		[[nodiscard]] constexpr bool isFinished() const noexcept{
			return done;
		}

		void compile(){
			//Windows Impl
			PROCESS_INFORMATION pi{};
			STARTUPINFO si{};
			si.cb = sizeof(si);
			si.wShowWindow = SW_HIDE;
			si.dwFlags = STARTF_USESHOWWINDOW;

			using namespace std::literals;

			targetPath = std::filesystem::exists(outputDirectory) ?
				std::filesystem::absolute(outputDirectory).string() : ""s;

			targetPath.append(std::format("{}.spv", overrideName.empty() ? sourcePath.filename().string() : overrideName));

			std::string cmdLine = std::format("{} -V {} -o {}",
					std::filesystem::absolute(compilerPath).string(),
					std::filesystem::absolute(sourcePath).string(),
					targetPath.string()
				);

			if(const bool fRet = CreateProcess(
				nullptr,
				cmdLine.data(),
				nullptr, nullptr,
				false,
				0,
				nullptr,
				R"(D:\projects\vulkan_framework\properties\shader)",
				&si, &pi); !fRet){
				std::println(std::cerr, "Unable To Execute GLSL Compiler Process");
			}

			WaitForSingleObject(pi.hProcess, INFINITE);

			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);

			done = true;
		}
	};
}

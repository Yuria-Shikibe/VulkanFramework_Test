export module StackTrace;

import std;

//TODO shits
export namespace ext{
	void getStackTraceBrief(std::ostream& ss, const bool jumpUnSource = true, const bool showExec = false, const int skipNative = 2){
		ss << "\n--------------------- Stack Trace Begin:\n\n";

		const auto currentStacktrace = std::stacktrace::current();

		int index = 0;
		for (const auto& entry : currentStacktrace) {
			if(entry.source_file().empty() && jumpUnSource) continue;
			if(entry.description().find("invoke_main") != std::string::npos && skipNative)break;

			index++;

			if(skipNative > 0 && skipNative >= index){
				continue;
			}

			auto exePrefix = entry.description().find_first_of('!');

			exePrefix = exePrefix == std::string::npos && showExec ? 0 : exePrefix + 1;

			auto charIndex = entry.description().find("+0x");

			std::filesystem::path src{entry.source_file()};

			std::string fileName = src.filename().string();

			if(!std::filesystem::directory_entry{src}.exists() || fileName.find('.') == std::string::npos) {
				// fileName.insert(0, "<").append(">");
				continue;
			}

			// std::string functionName = entry.description().substr(index, charIndex - index);
			std::string functionPtr = entry.description().substr(charIndex + 1);

			ss << std::left << "[SOURCE   FILE] : " << fileName << '\n';
			ss << std::left << "[FUNC     NAME] : " << entry.description().substr(exePrefix) << '\n';
			ss << std::left << "[FUNC END LINE] : " << entry.source_line() << " [FUNC PTR]: " << std::dec << functionPtr << "\n\n";
		}

		ss << "--------------------- Stack Trace End:\n\n";
	}
}
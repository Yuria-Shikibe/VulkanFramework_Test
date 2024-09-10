module ext.stack_trace;

import ext.algo.string_parse;
import ext.meta_programming;

void ext::print_stack_trace(std::ostream& ss, unsigned skip){
	const auto currentStacktrace = std::stacktrace::current();
	std::println(ss, "Stack Trace: ");

	for (const auto & [i, stacktrace] : currentStacktrace
	| std::views::filter([](const std::stacktrace_entry& entry){
		const std::filesystem::path path = entry.source_file();
		return std::filesystem::exists(path) && !path.extension().empty();
	})
	| std::views::drop(skip)
	| std::views::enumerate){
		auto desc = stacktrace.description();
		auto descView = std::string_view{desc};

#if defined(_MSC_VER) || defined(_WIN32)
		//MSVC ONLY
		const auto begin = descView.find('!');
		descView.remove_prefix(begin + 1);
		const auto end = descView.rfind('+');
		descView.remove_suffix(descView.size() - end);
#endif

		std::print(ss, "[{:_>3}] {}({})\n\t", i, stacktrace.source_file(), stacktrace.source_line() - 1);

		if(auto rst2 = algo::unwrap(descView, '`'); rst2.is_flat()){
			std::print(ss, "{}", descView);
		}else{
			static constexpr std::string_view Splitter{"::"};
			for (auto layers = rst2.get_clean_layers(); const auto & basic_string : layers){
				bool spec = basic_string.ends_with(Splitter);

				auto splited = basic_string | std::views::split(Splitter);

				if(spec){
					for(const auto& basic_string_view : splited
						| std::views::transform(ext::to<std::string_view>{})
						| std::views::drop_while(ext::algo::begin_with_digit)
						| std::views::take(1)
					){
						std::print(ss, "{}", basic_string_view, spec ? "->" : "");
					}
				} else{
					for(const auto& basic_string_view : splited
						| std::views::transform(ext::to<std::string_view>{})
						| std::views::drop_while(ext::algo::begin_with_digit)
					){
						std::print(ss, "::{}", basic_string_view, spec ? "->" : "");
					}
				}
			}
		}

		std::println(ss, "\n");
	}

	std::println(ss, "\n");
}

void ext::getStackTraceBrief(std::ostream& ss, const bool jumpUnSource, const bool showExec, const int skipNative){
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

export module Core.FilePlain;

export import Core.File;

import ext.heterogeneous;
import ext.meta_programming;
import std;

export namespace Core{
	struct FilePlane{
		[[nodiscard]] FilePlane() = default;

		[[nodiscard]] explicit FilePlane(const File& rootDir)
			: rootDir{rootDir}{
			build();
		}

		[[nodiscard]] const File& root() const noexcept{
			return rootDir;
		}

		[[nodiscard]] const ext::string_hash_map<File>& flat() const noexcept{
			return flatFiles;
		}

		[[nodiscard]] const ext::string_hash_map<File>& dirs() const noexcept{
			return directories;
		}

		[[nodiscard]] File findDir(const std::string_view name) const {
			const auto it = std::ranges::find_if(directories, [&](const File& o){
				return o.getPath().filename() == name;
			}, &decltype(directories)::value_type::first);

			if(it == directories.end()){
				File file = rootDir / name;
				return file;
			}

			return it->second;
		}

	private:
		File rootDir{};

		ext::string_hash_map<File> flatFiles{};
		ext::string_hash_map<File> directories{};

		void build(){
			for(File&& file : std::filesystem::recursive_directory_iterator(rootDir.getPath()) | std::views::transform(ext::convert_to<File>{})){
				std::pair<ext::string_hash_map<File>::iterator, bool> rst{};

				if(file.isDir()){
					rst = directories.try_emplace(file.filename(), file);
				}else{
					rst = flatFiles.try_emplace(file.filename(), file);
				}

				if(!rst.second){
					std::println(std::cerr, "duplicated file {} : {} | {}",
						file.filename(), file.absolutePath().string(), rst.first->second.absolutePath().string());
					throw std::runtime_error{"duplicated file exception"};
				}
			}
		}
	};
}

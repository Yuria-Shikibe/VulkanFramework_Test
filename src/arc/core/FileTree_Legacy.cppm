export module Core.FileTreeLegacy;

export import Core.File;
import ext.RuntimeException;
import std;
import ext.heterogeneous;

export namespace Core{
	//TODO use a struct to wrap the data structure of just directly use vector?
	//TODO redisgn this, mess

	/**
	 * \brief Uses to operate frequently used `readonly` files, static!
	 * */
	class FileTree_Legacy{
	protected:
		/**
		 * \brief empty for add all subs
		 * */
		std::vector<File::Filter> concentration{};

		std::vector<File> subDirectories{};

		/**
		 * \brief Key - directory name | Value - File
		 * */
		ext::string_hash_map<std::vector<File>> files{};

		ext::string_hash_map<File> flatView{};

		File root{};

		static void output(std::ostream &os, const File& file, const unsigned char depth){
			if(depth == 0){
				os << file.getPath().parent_path().string() << "/\n";
			}

			for (int i = 0; i < depth; ++i) {
				os << "  |\t";
			}

			os << file << '\n';

			if(file.isDir()){
				file.forSubs([d = depth + 1, &os](File&& f){
					output(os, f, d);
				});
			}
		}


	public:
		[[nodiscard]] bool concentrateAll() const{
			return concentration.empty();
		}

		FileTree_Legacy() = default;

		explicit FileTree_Legacy(const std::string_view& root) : root{root} {buildFileTree();}

		explicit FileTree_Legacy(const File& root) : root{root} {buildFileTree();}

		explicit FileTree_Legacy(File&& root) : root{std::move(root)} {buildFileTree();}

		friend std::ostream &operator<<(std::ostream &os, const FileTree_Legacy& tree) {
			output(os, tree.root, 0);

			os << '\n';

			return os;
		}

		[[nodiscard]] const ext::string_hash_map<std::vector<File>>& getFiles() const{
			return files;
		}

		[[nodiscard]] const std::vector<File>& getDirs() const{
			return subDirectories;
		}

		template<typename ...T>
			requires (std::is_same_v<T, File::Filter> && ...)
		void registerConcentration(T&&... args){
			(concentration.push_back(std::forward<T>(args)), ...);
		}


		template <bool ignoreDir = true, typename Func>
			requires (ext::invocable<std::string(const File&), Func>)
		void mapSubFiles(Func&& func) {
			for(auto& element : files | std::ranges::views::values) {
				for(auto& file : element) {
					if constexpr (ignoreDir){
						if(file.isDir())continue;
					}

					if(const auto [itr, success] = flatView.try_emplace(func(file), file); !success) {
						std::println(std::cerr, "It's illegal to map file tree that has files with the same stem name! {}", itr->first);
						throw std::invalid_argument{"duplicated file stem name"};
					}
				}
			}
		}

		[[nodiscard]] ext::string_hash_map<File>& getFlatView(){ return flatView; }

		template <bool quiet = false>
		[[nodiscard]] File flatFind(const std::string_view fileName) const {
			if(const auto itr = flatView.find(fileName); itr != flatView.end()){
				return itr->second;
			}

			if constexpr(quiet){
				return {};
			}else{
				std::println(std::cerr, "Failed To Find File: {}", fileName);
				throw std::invalid_argument{"file not found"};
			}

		}

		void reDirect(const File& rootFile){
			root = rootFile;

			files.clear();
		}

		void buildFileTree(const bool clearLegacy = false){
			if(!root.exist() || !root.isDir()) {
				throw std::invalid_argument{"Cannot Setup A FileTree Based On A Non-Dir File!"};
			}

			if(clearLegacy)files.clear();

			if(concentrateAll()){
				files = std::move(root.sortSubs<true>());
			}else{
				files = std::move(root.sortSubsBy<true>(concentration));
			}

			for (const auto noExtensions = files[static_cast<std::string>(File::EMPTY_EXTENSION)]; const auto& file : noExtensions) {
				if(file.isDir())subDirectories.push_back(file);
			}
		}

		File findDir(const std::string_view name){
			const auto it = std::ranges::find_if(subDirectories, [&name](const File& o){
				return o.filename() == name;
			});

			if(it == subDirectories.end()){
				File file = root.subFile(name);
				file.createDirQuiet();
				return file;
			}

			return *it;
		}

		[[nodiscard]] const std::vector<File>& find(const std::string_view category) const{
			return files.at(category);
		}

		[[nodiscard]] File findAbsolute(const std::string_view fileName) const{
			return root.subFile(fileName);
		}

		/**
		 * TODO this function is a totally disaster
		 * \brief Return root if find nothing!
		 * */
		[[nodiscard]] File find(const std::string_view fileName, const std::string_view category = "") const {
			if(category.empty()){
				auto values = files | std::ranges::views::values;

				std::vector<File>::const_iterator index2;

				if(const auto index1 = std::ranges::find_if(values, [&fileName, &index2](const std::vector<File>& fileVector){
					index2 = std::ranges::find_if(fileVector, [&fileName](const File& file){
						return fileName == file.filename();
					});

					return index2 != fileVector.end();
				}); index1 != values.end() && index2 != (*index1).end()){
					return *index2;
				}

				return root;
			}

			if(!files.contains(category))return root;
			auto sameTypes = files.at(category);

			if(const auto result = std::ranges::find_if(sameTypes, [&fileName](const File& file){
				return fileName == file.filename();
			}); result != sameTypes.end()){
				return *result;
			}

			return root;
		}
	};

}
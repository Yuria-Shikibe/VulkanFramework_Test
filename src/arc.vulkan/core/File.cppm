export module Core.File;

import ext.Concepts;
import ext.RuntimeException;
import ext.Heterogeneous;

import std;

namespace fs = std::filesystem;

namespace Core{
	export class File{
	protected:
		fs::path rawPath{};

	public:
		static constexpr std::string_view EMPTY_EXTENSION = "[empty]";

		File() = default;

		explicit(false) File(const decltype(rawPath)::string_type& p) : rawPath{p}{}

		explicit(false) File(const std::string& p) : rawPath{p}{}

		explicit(false) File(decltype(rawPath)::string_type&& p) : rawPath{std::move(p)}{}

		explicit(false) File(const char* p) : rawPath{p}{}

		explicit(false) File(const std::string_view p) : rawPath{p}{}

		explicit(false) File(const fs::path& p) : rawPath{p}{}

		explicit(false) File(fs::path&& p) : rawPath{std::move(p)}{}

		explicit(false) File(const fs::directory_entry& p) : rawPath(p){}

		File& operator=(const fs::path& other){
			rawPath = other;
			return *this;
		}

		File& operator=(fs::path&& other){
			rawPath = std::move(other);
			return *this;
		}

		explicit operator fs::directory_entry() const{
			return fs::directory_entry{rawPath};
		}

		operator const fs::path&() const{
			return rawPath;
		}

		friend bool operator==(const File& lhs, const File& rhs){
			return lhs.rawPath == rhs.rawPath;
		}

		friend bool operator!=(const File& lhs, const File& rhs){
			return !(lhs == rhs);
		}

		[[nodiscard]] auto getFileSize() const{
			return fs::file_size(rawPath);
		}

		[[nodiscard]]fs::path absolutePath() const{
			return absolute(getPath());
		}

		[[nodiscard]] std::string extension() const{
			return rawPath.extension().string();
		}

		[[nodiscard]] std::string stem() const{
			return rawPath.stem().string();
		}

		[[nodiscard]] std::string filename() const{
			return rawPath.filename().string();
		}

		[[nodiscard]] std::string filenameFull() const{
			std::string name = rawPath.filename().string();
			if(filename().empty()){
				name = absolutePath().string();
			}
			return (isDir() ? "<Dir> " : "<Fi> ") + name;
		}

		[[nodiscard]] std::string filenameFullPure() const{
			std::string name = rawPath.filename().string();
			if(filename().empty()){
				name = absolutePath().string();
			}
			return name;
		}

		[[nodiscard]] fs::path& getPath(){
			return rawPath;
		}

		[[nodiscard]] const fs::path& getPath() const{
			return rawPath;
		}

		[[nodiscard]] bool exist() const{
			return exists(rawPath);
		}

		[[nodiscard]] bool deleteFile() const{
			return exist() && (isDir()
				                   ? remove(absolutePath())
				                   : remove_all(absolutePath()));
		}

		void deleteFileQuiet() const{
			(void)deleteFile();
		}

		[[nodiscard]] bool copy(const fs::path& dest) const{
			try{
				fs::copy(getPath(), dest);
				return true;
			} catch([[maybe_unused]] std::error_code& ignore){
				return false;
			}
		}

		[[nodiscard]] bool copy(const File& dest) const{
			return copy(dest.getPath());
		}

		[[nodiscard]] bool isDir() const{
			return is_directory(getPath());
		}

		[[nodiscard]] bool isRegular() const{
			return is_regular_file(getPath());
		}

		[[nodiscard]] bool isHidden() const{
			return stem().starts_with('.');
		}

		[[nodiscard]] bool emptyExtension() const{
			return extension().empty();
		}

		[[nodiscard]] bool createDir(const bool autoCreateParents = true) const{
			return autoCreateParents ? create_directories(getPath()) : create_directory(getPath());
		}

		void createDirQuiet(const bool autoCreateParents = true) const{
			(void)createDir(autoCreateParents);
		}

		[[nodiscard]] bool createFile(const bool autoCreateParents = false) const{
			if(exist()) return false;

			if(autoCreateParents){
				if(const File parent = File{getParent()}; !parent.exist()){
					(void)parent.createDir();
				}
			}

			const std::ofstream ofStream(getPath());
			const bool valid = ofStream.is_open();

			return valid;
		}

		void createFileQuiet(const bool autoCreateParents = false) const{
			(void)createFile(autoCreateParents);
		}

		[[nodiscard]] File getParent() const{
			return File{rawPath.parent_path()};
		}

		[[nodiscard]] File getRoot() const{
			return File{rawPath.root_path()};
		}

		[[nodiscard]] bool hasParent() const{
			return rawPath.has_parent_path();
		}

		[[nodiscard]] bool hasExtension() const{
			return rawPath.has_extension();
		}

		File& gotoParent(){
			rawPath = rawPath.parent_path();

			return *this;
		}

		[[nodiscard]] File subFile(const std::string_view name) const{
			if(!isDir()) throw ext::RuntimeException{};

			return File{absolutePath().append(name)};
		}

		[[nodiscard]] File subFile(const std::string& name) const{
			if(!isDir()) throw ext::RuntimeException{};

			return File{absolutePath().append(name)};
		}

		[[nodiscard]] File subFile(const fs::path& path) const{
			if(!isDir()) throw ext::RuntimeException{};

			return File{absolutePath() / path};
		}

		[[nodiscard]] File subFile(const char* name) const{
			if(!isDir()) throw ext::RuntimeException{};

			return File{absolutePath().append(name)};
		}

		[[nodiscard]] File find(const std::string_view name) const{
			if(File ret = subFile(name); ret.exist()){
				return ret;
			}

			throw ext::RuntimeException{std::format("Unable To Find File `{}` in Dir <{}>", name, absolutePath().string())};
		}

		/**
		 * Warning: This does not do append but erase and write!
		 * */
	    template <std::ranges::contiguous_range Rng>
		void writeByte(const Rng& range) const{
			if(std::ofstream ofStream(getPath(), std::ios::binary); ofStream.is_open()){
			    ofStream.write(reinterpret_cast<const char *>(std::ranges::data(range)), sizeof(std::ranges::range_value_t<Rng>) * std::ranges::size(range));
			}
		}

	    /**
		 * Warning: This does not do append but erase and write!
		 * */
		void writeString(const std::string_view string) const{
			if(std::ofstream ofStream(getPath()); ofStream.is_open()){
			    ofStream << string;
			}
		}

		template <bool careDirs = false>
		[[nodiscard]] std::vector<File> subs() const{
			std::vector<File> files;
			for(const auto& item : fs::directory_iterator(getPath())){
				if constexpr (careDirs){
					files.emplace_back(item);
				}else{
					if(!item.is_directory()){
						files.emplace_back(item);
					}
				}
			}

			return files;
		}

		template <ext::Invokable<bool(const File&)> Pred>
		[[nodiscard]] std::vector<File> subs(Pred&& pred) const{
			std::vector<File> files;
			for(const auto& item : fs::directory_iterator(getPath())){
				File file{item};
				if (pred(file)){
					files.push_back(std::move(file));
				}
			}

			return files;
		}

		void forSubs(std::invocable<File&&> auto&& consumer) const{
			for(const auto& item : fs::directory_iterator(getPath())){
				consumer(File{item});
			}
		}

		template <bool careDirs = false>
		void forAllSubs(std::invocable<File&&> auto&& consumer) const{
			for(const auto& item : fs::recursive_directory_iterator(getPath())){
				if(File f{item}; f.isRegular()){
					consumer(std::move(f));
				} else{
					if constexpr (careDirs){
						consumer(std::move(f));
					}
				}
			}
		}

		template <bool careDirs = false>
		void allSubs(std::vector<File>& container) const{
			for(const auto& item : fs::recursive_directory_iterator(getPath())){
				if(File f{item}; f.isRegular()){
					container.emplace_back(std::move(f));
				} else{
					if constexpr(careDirs){
						container.emplace_back(std::move(f));
					}
				}
			}
		}

		template <ext::InvokeNullable<void(std::string&)> Consumer = std::nullptr_t>
		[[nodiscard]] std::string readString_byLine(Consumer&& consumer = nullptr) const{
			std::ifstream file_stream(getPath());

			if(!file_stream.is_open()) return "";

			std::ostringstream file_contents;
			std::string line;

			while(std::getline(file_stream, line)){
				if constexpr (!std::same_as<std::nullptr_t, Consumer>){
					consumer(line);
				}

				file_contents << line;
				if(!file_stream.eof()){
					file_contents << '\n';
				}
			}

			return std::move(file_contents).str();
		}

		[[nodiscard]] std::string readString(std::size_t padding = 1) const{
			std::ifstream file_stream(getPath(), std::ios::binary | std::ios::ate);
			if(!file_stream.is_open()) return "";

			const std::size_t length = file_stream.tellg();
			file_stream.seekg(std::ios::beg);

			std::string str{};

			str.resize_and_overwrite(length, [&file_stream](char* buf, const std::size_t buf_size) noexcept{
				file_stream.read(buf, static_cast<std::streamsize>(buf_size));
				return buf_size;
			});

			return str;
		}

	    template <typename T = std::byte>
		[[nodiscard]] std::vector<T> readBytes() const{
			std::ifstream file_stream(getPath(), std::ios::binary | std::ios::ate);
			if(!file_stream.is_open()) {
                throw ext::RuntimeException{std::format("Failed to open file '{}'", filename()) };
			};

			const std::size_t length = file_stream.tellg();
			file_stream.seekg(std::ios::beg);

			std::vector<T> str((length - 1) / sizeof(T) + 1);

		    file_stream.read(reinterpret_cast<char *>(str.data()), static_cast<std::streamsize>(str.size() * sizeof(T)));

			return str;
		}

		void writeByte(std::invocable<std::ofstream&> auto&& func){
			if(std::ofstream ofStream(absolutePath(), std::ios::binary); ofStream.is_open()){
				func(ofStream);
			}
		}

		friend std::ostream& operator<<(std::ostream& os, const File& file){
			os << file.filenameFull();
			return os;
		}

		using Filter = std::pair<std::string, std::function<bool(const File&)>>;

		template <bool careDirs = false>
		[[nodiscard]] ext::StringHashMap<std::vector<File>> sortSubs() const{
			ext::StringHashMap<std::vector<File>> map;
			this->forAllSubs<careDirs>([&map](File&& file){
				const std::string& extension = file.extension();
				map[extension.empty() ? static_cast<std::string>(EMPTY_EXTENSION) : extension].push_back(file);
			});

			return map;
		}

		template <bool careDirs = false>
		[[nodiscard]] ext::StringHashMap<std::vector<File>> sortSubsBy(const std::span<Filter>& standards) const{
			ext::StringHashMap<std::vector<File>> map;

			this->forAllSubs<careDirs>([&standards, &map](File&& file){
				if(const auto it = std::ranges::find_if(standards, [&file](const Filter& pair){
					return pair.second(file);
				}); it != standards.end()){
					map[it->first].push_back(file);
				}
			});

			return map;
		}

		static ext::StringHashMap<std::vector<File>> sortBy(
			const std::span<File>& files, const std::span<Filter>& standards){
			ext::StringHashMap<std::vector<File>> map;

			for(const File& file : files){
				if(
					auto it = std::ranges::find_if(standards, [&file](const Filter& pair){
						return pair.second(file);
					});
					it != standards.end()
				){
					map[it->first].push_back(file);
				}
			}

			return map;
		}

		friend auto operator<=>(const File& lhs, const File& rhs){
			if(lhs.isDir()){
				if(rhs.isDir()){
					return lhs.rawPath <=> rhs.rawPath;
				}
				return std::strong_ordering::less;
			}

			if(rhs.isDir()){
				return std::strong_ordering::greater;
			}

			return lhs.rawPath <=> rhs.rawPath;
		}

		friend File operator/(const File& l, const File& file){
			return File{l.rawPath / file.getPath()};
		}

		friend File operator/(const File& l, const std::string_view path){
			return File{l.rawPath / path};
		}

		friend File operator/(const File& l, const char* path){
			return File{l.rawPath / path};
		}

		File& operator/=(const File& file){
			rawPath /= file.getPath();
			return *this;
		}

		File& operator/=(const std::string_view path){
			rawPath /= path;
			return *this;
		}

		File& operator/=(const char* path){
			rawPath /= path;
			return *this;
		}

	};

	export
	/**
	 * @brief default is ascend;
	 */
	enum struct FileSortFunc : unsigned char{
		name = 0b0000'0001,
		time = 0b0000'0010,
		size = 0b0000'0100,

		/**	default 'ascend', it here does actually nothing*/
		ascend = 0,
		descend = 0b0001'0000
	};

	export FileSortFunc operator|(FileSortFunc l, FileSortFunc r) noexcept{
		return FileSortFunc{static_cast<std::underlying_type_t<FileSortFunc>>(std::to_underlying(l) | std::to_underlying(r))};
	}

	export bool operator&(FileSortFunc l, FileSortFunc r) noexcept{
		return static_cast<bool>(std::to_underlying(l) | std::to_underlying(r));
	}
}



export namespace Core{
	std::function<bool(const File&, const File&)> getFileSortter(FileSortFunc sortFunc){
		using Func = std::function<bool(const File&, const File&)>;
		const auto opTy = FileSortFunc{static_cast<std::underlying_type_t<FileSortFunc>>(sortFunc) & 0x0fu};

		const bool descend = sortFunc & FileSortFunc::descend;

		switch(opTy){
			case FileSortFunc::name:{
				return descend ? Func{std::greater<File>{}} : Func{std::less<File>{}};
			}
			case FileSortFunc::size:{
				return descend ? [](const File& l, const File& r){
					return l.getFileSize() < r.getFileSize();
				} : [](const File& l, const File& r){
					return l.getFileSize() > r.getFileSize();
				};
			}
			case FileSortFunc::time:{
				return descend ? [](const File& l, const File& r){
					return fs::last_write_time(l.getPath()) < fs::last_write_time(r.getPath());
				} : [](const File& l, const File& r){
					return fs::last_write_time(l.getPath()) > fs::last_write_time(r.getPath());
				};
			}

			default: return descend ? Func{std::greater<File>{}} : Func{std::less<File>{}};
		}
	}
}

export
template <>
struct std::hash<Core::File>{
	std::size_t operator()(const Core::File& p) const noexcept{
		static constexpr std::hash<std::filesystem::path> hasher{};
		return hasher(p.getPath());
	}
};

export
template <>
struct std::formatter<Core::File>{
	template <typename Context>
	constexpr auto parse(Context& context) const{
		return context.begin();
	}

	auto format(const Core::File& p, auto& context) const{
		return std::format_to(context.out(), "{}", p.filenameFull());
	}
};
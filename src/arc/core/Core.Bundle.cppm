export module Core.Bundle;

export import ext.json;
export import Core.File;

import std;
import ext.heterogeneous;

namespace Core{

	[[nodiscard]] decltype(auto) spilt(const std::string_view key){
		return key
			| std::views::split('.')
			| std::views::transform([](auto c){ return std::string_view{c}; });
	}


	template <std::ranges::bidirectional_range Rng>
		requires (std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>)
	void printNotFound(Rng&& rng, std::string_view current, const ext::json::jval_tag tag){
		std::print(std::cerr, "Bundle: ");

		auto last = std::ranges::prev(std::ranges::end(rng));

		for(const std::string_view& strs : std::ranges::subrange{std::ranges::begin(rng), last}){
			std::print(std::cerr, "{}.", strs);
		}

		std::println(std::cerr, "{} Not Found\nAt: {}, which is {}", *last, current, std::to_underlying(tag));
	}

	template <std::ranges::forward_range Rng>
		requires (std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>)
	void printNotFound(Rng&& rng, std::string_view current, const ext::json::jval_tag tag){
		std::print(std::cerr, "Bundle: ");

		for(const std::string_view& strs : std::ranges::subrange{std::ranges::begin(rng), std::ranges::end(rng)}){
			std::print(std::cerr, "{}.", strs);
		}

		std::println(std::cerr, " Not Found\nAt: {}, which is {}", current, std::to_underlying(tag));
	}


	template <std::ranges::input_range Rng>
		requires (std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>)
	void printNotFound(Rng&& rng, std::string_view current, const ext::json::jval_tag tag){
		std::println(std::cerr, "Bundle Not Found\nAt: {}, which is {}", current, std::to_underlying(tag));
	}

	export class Bundle;
	struct BundleLoadable{
		virtual ~BundleLoadable() = default;

		virtual void loadBundle(const Bundle* bundle) = 0;
	};

	export class Bundle{
	public:
		static constexpr std::string_view Locale = "locale";
		static constexpr std::string_view NotFound = "???Not_Found???";
		static constexpr std::string_view Null = "N/A";

	private:
		std::vector<BundleLoadable*> bundleRequesters{};

		ext::json::json_value currentBundle{};
		ext::json::json_value fallbackBundle{};
		std::locale currentLocale{};

		static ext::json::json_value loadFile(const File& file){
			return ext::json::parse(file.readString());
		}

		template <std::ranges::input_range Rng>
			requires (std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>)
		static std::optional<std::string_view> find(Rng&& dir, const ext::json::Object* current) noexcept{
			for(const std::string_view& cur : dir){
				if(const auto itr = current->try_find(cur)){
					switch(auto tag = itr->get_tag()){
						case ext::json::object :{
							current = &itr->as_obj();
							break;
						}

						case ext::json::string :{
							return itr->as<std::string>();
						}

						case ext::json::null :{
							return Null;
						}

						default:{
							Core::printNotFound(std::forward<Rng>(dir), cur, tag);
							break;
						}
					}
				}else{
					Core::printNotFound(std::forward<Rng>(dir), cur, ext::json::null);
				}
			}

			return std::nullopt;
		}

	public:
		[[nodiscard]] Bundle(){
			currentBundle.as_obj();
		}

		[[nodiscard]] explicit Bundle(const File& file){
			load(file);
		}

		[[nodiscard]] Bundle(const File& file, const File& fallback){
			load(file, fallback);
		}

		[[nodiscard]] const std::locale& getLocale() const{
			return currentLocale;
		}

		ext::json::Object& getBundles(ext::json::json_value Bundle::* ptr = &Bundle::currentBundle){
			return (this->*ptr).as_obj();
		}

		[[nodiscard]] const ext::json::Object& getBundles(ext::json::json_value Bundle::* ptr = &Bundle::currentBundle) const{
			return (this->*ptr).as_obj();
		}

		void loadFallback(ext::json::json_value&& jsonValue){
			fallbackBundle = std::move(jsonValue);
			fallbackBundle.as_obj();
		}

		void load(ext::json::json_value&& jsonValue){
			currentBundle = std::move(jsonValue);
			currentBundle.as_obj();
			for(const auto bundleRequester : bundleRequesters){
				bundleRequester->loadBundle(this);
			}
		}

		void loadFallback(const File& file){
			loadFallback(loadFile(file));
		}

		void load(const File& file){
			load(loadFile(file));
		}

		void load(const File& target, const File& fallback){
			load(target);
			loadFallback(fallback);
		}

		template <std::ranges::range Rng = std::initializer_list<std::string_view>>
			requires (std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>)
		[[nodiscard]] std::optional<std::string_view> find(Rng&& keyWithConstrains) const noexcept{
			auto rng = keyWithConstrains | std::views::transform(spilt) | std::views::join;
			auto* last = &currentBundle.as_obj();

			std::optional<std::string_view> rst = Bundle::find(rng, last);

			if(!rst){
				last = fallbackBundle.try_get<ext::json::object>();
				if(last) rst = Bundle::find(rng, last);
			}

			return rst;
		}

		template <std::ranges::range Rng = std::initializer_list<std::string_view>>
			requires (std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>)
		[[nodiscard]] std::string_view find(Rng&& keyWithConstrains, const std::string_view def) const noexcept{
			const std::optional<std::string_view> rst = this->find(std::forward<Rng>(keyWithConstrains));
			return rst.value_or(def);
		}

		[[nodiscard]] std::string_view find(const std::string_view key, const std::string_view def) const noexcept{
			auto dir = spilt(key);
			auto* last = &currentBundle.as_obj();

			auto rst = Bundle::find(dir, last);

			if(!rst){
				last = &fallbackBundle.as_obj();
				rst = find(dir, last);
			}

			return rst.value_or(def);
		}

		[[nodiscard]] std::string_view find(const std::string_view key) const noexcept{
			return find(key, key);
		}

		template <typename... T>
		[[nodiscard]] std::string format(const std::string_view key, T&&... args) const{
			return std::vformat(find(key), std::make_format_args(std::forward<T>(args)...));
		}

		[[nodiscard]] std::string_view operator[](const std::string_view key) const noexcept{
			return find(key, key);
		}

		template <std::ranges::range Rng = std::initializer_list<std::string_view>>
			requires (std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>)
		[[nodiscard]] std::string_view operator[](Rng&& keys) const noexcept{
			if constexpr (std::ranges::bidirectional_range<Rng>){
				if(std::ranges::empty(keys)){
					return NotFound;
				}else{
					return this->find(std::forward<Rng>(keys), *std::ranges::rbegin(keys));
				}
			}else{
				return this->find(std::forward<Rng>(keys), NotFound);
			}
		}
	};
}

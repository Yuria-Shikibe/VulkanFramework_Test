export module Assets.Bundle;

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
	void printNotFound(Rng&& rng, std::string_view current, const ext::json::JsonValueTag tag){
		std::print(std::cerr, "Bundle: ");

		auto last = std::ranges::prev(std::ranges::end(rng));

		for(const std::string_view& strs : std::ranges::subrange{std::ranges::begin(rng), last}){
			std::print(std::cerr, "{}.", strs);
		}

		std::println(std::cerr, "{} Not Found\nAt: {}, which is {}", *last, current, std::to_underlying(tag));
	}

	template <std::ranges::forward_range Rng>
		requires (std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>)
	void printNotFound(Rng&& rng, std::string_view current, const ext::json::JsonValueTag tag){
		std::print(std::cerr, "Bundle: ");

		for(const std::string_view& strs : std::ranges::subrange{std::ranges::begin(rng), std::ranges::end(rng)}){
			std::print(std::cerr, "{}.", strs);
		}

		std::println(std::cerr, " Not Found\nAt: {}, which is {}", current, std::to_underlying(tag));
	}


	template <std::ranges::input_range Rng>
		requires (std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>)
	void printNotFound(Rng&& rng, std::string_view current, const ext::json::JsonValueTag tag){
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

		ext::json::JsonValue currentBundle{};
		ext::json::JsonValue fallbackBundle{};
		std::locale currentLocale{};

		static ext::json::JsonValue loadFile(const File& file){
			return ext::json::parse(file.readString());
		}

		template <std::ranges::input_range Rng>
			requires (std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>)
		static std::optional<std::string_view> find(Rng&& dir, const ext::json::Object* current) noexcept{
			for(const std::string_view& cur : dir){
				if(const auto itr = current->try_find(cur)){
					switch(auto tag = itr->getTag()){
						case ext::json::object :{
							current = &itr->asObject();
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
			currentBundle.asObject();
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

		ext::json::Object& getBundles(ext::json::JsonValue Bundle::* ptr = &Bundle::currentBundle){
			return (this->*ptr).asObject();
		}

		[[nodiscard]] const ext::json::Object& getBundles(ext::json::JsonValue Bundle::* ptr = &Bundle::currentBundle) const{
			return (this->*ptr).asObject();
		}

		void loadFallback(ext::json::JsonValue&& jsonValue){
			fallbackBundle = std::move(jsonValue);
			fallbackBundle.asObject();
		}

		void load(ext::json::JsonValue&& jsonValue){
			currentBundle = std::move(jsonValue);
			currentBundle.asObject();
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
		[[nodiscard]] std::string_view find(Rng&& keyWithConstrains, const std::string_view def = NotFound) const noexcept{
			auto rng = keyWithConstrains | std::views::transform(spilt) | std::views::join;
			auto* last = &currentBundle.asObject();

			std::optional<std::string_view> rst = Bundle::find(rng, last);

			if(!rst){
				last = fallbackBundle.tryGetValue<ext::json::object>();
				if(last)rst = Bundle::find(rng, last);
			}

			return rst.value_or(def);
		}

		[[nodiscard]] std::string_view find(const std::string_view key, const std::string_view def) const noexcept{
			auto dir = spilt(key);
			auto* last = &currentBundle.asObject();

			auto rst = Bundle::find(dir, last);

			if(!rst){
				last = &fallbackBundle.asObject();
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
			return this->find(std::forward<Rng>(keys), NotFound);
		}
	};
}

export module Assets.Bundle;

export import ext.json;
export import Core.File;

import std;
import ext.Heterogeneous;

namespace Core{

	[[nodiscard]] decltype(auto) spilt(const std::string_view key){
		return key
			| std::views::split('.')
			| std::views::transform([](auto c){ return std::string_view{c}; });
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

	private:
		std::vector<BundleLoadable*> bundleRequesters{};

		ext::json::JsonValue currentBundle{};
		ext::json::JsonValue fallbackBundle{};
		std::locale currentLocale{};


		//TODO return optional?
		[[nodiscard]] const auto& getCategory(const std::string_view category) const{
			const auto& map = getBundles(&Bundle::currentBundle);

			if(const auto itr = map.find(category); itr != map.end()){
				if(itr->second.is<ext::json::Object>()) return itr->second.asObject();
			}

			return map;
		}

		static ext::json::JsonValue loadFile(const File& file){
			return ext::json::parse(file.readString());
		}

		template <std::ranges::range Rng>
			requires (std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>)
		static std::optional<std::string_view> find(Rng&& dir, const ext::json::Object* last){
			for(const std::string_view& cates : dir){
				if(const auto itr = last->find(cates); itr != last->end()){
					if(itr->second.is<ext::json::object>()){
						last = &itr->second.asObject();
					} else if(itr->second.is<ext::json::string>()){
						return itr->second.as<std::string>();
					} else{
						break;
					}
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

		[[nodiscard]] const std::locale& getLocale() const{ return currentLocale; }

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
		[[nodiscard]] std::string_view find(Rng&& keyWithConstrains, const std::string_view def = NotFound) const{
			auto rng = keyWithConstrains | std::views::transform(spilt) | std::views::join;
			auto* last = &currentBundle.asObject();

			std::optional<std::string_view> rst = this->find(rng, last);

			if(!rst){
				last = &fallbackBundle.asObject();
				rst = this->find(rng, last);
			}

			return rst.value_or(def);
		}

		[[nodiscard]] std::string_view find(const std::string_view key, const std::string_view def) const{
			auto dir = spilt(key);
			auto* last = &currentBundle.asObject();

			auto rst = find(dir, last);

			if(!rst){
				last = &fallbackBundle.asObject();
				rst = find(dir, last);
			}

			return rst.value_or(def);
		}

		[[nodiscard]] std::string_view find(const std::string_view key) const{
			return find(key, key);
		}

		template <typename... T>
		[[nodiscard]] std::string format(const std::string_view key, T&&... args) const{
			return std::vformat(find(key), std::make_format_args(std::forward<T>(args)...));
		}

		[[nodiscard]] std::string_view operator[](const std::string_view key) const{
			return find(key, key);
		}

		template <std::ranges::range Rng = std::initializer_list<std::string_view>>
			requires (std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>)
		[[nodiscard]] std::string_view operator[](Rng&& keys) const{
			return this->find(std::forward<Rng>(keys), NotFound);
		}
	};
}

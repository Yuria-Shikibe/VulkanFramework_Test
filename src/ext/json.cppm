module;

#include <cerrno>

export module ext.json;

import std;
import ext.heterogeneous;
import ext.meta_programming;

namespace ext::json{
	export struct parse_string_t{};
	export constexpr parse_string_t parse_string{};

	export class json_value;

	export using Integer = std::int64_t;
	export using Float = std::double_t;
	export using Object = string_hash_map<json_value>;
	export using Array = std::vector<json_value>;

	export template <typename T>
	using json_scalar_t =
		std::conditional_t<std::same_as<T, bool>, bool,
		std::conditional_t<std::same_as<T, std::nullptr_t>, std::nullptr_t,
		std::conditional_t<std::is_floating_point_v<T>, Float, Integer>>>;

	export template <typename T>
	using JsonArithmeticType = std::conditional_t<std::is_floating_point_v<T>, Float, Integer>;

	export namespace keys{
		/**
		 * @brief Indeicate this json info refer to a Polymorphic Class
		 */
		constexpr std::string_view Typename = "$ty"; //type
		constexpr std::string_view Tag = "$tag";       //tag
		constexpr std::string_view ID = "$id";        //id
		constexpr std::string_view Data = "$d";      //data
		constexpr std::string_view Version = "$ver";   //version
		constexpr std::string_view Pos = "$pos";       //position
		constexpr std::string_view Size2D = "$sz2";   //size 2D
		constexpr std::string_view Offset = "$off";   //offset
		constexpr std::string_view Trans = "$trs";   //transform
		constexpr std::string_view Size = "$sz1";     //size 1D
		constexpr std::string_view Bound = "$b";     //bound

		constexpr std::string_view Key = "$key";   //key
		constexpr std::string_view Value = "$val"; //value

		constexpr std::string_view First = "$fst";   //first
		constexpr std::string_view Secound = "$sec"; //second
		constexpr std::string_view Count = "$cnt"; //second
	}

	struct [[deprecated]] IllegalJsonSegment final : std::exception{
		IllegalJsonSegment() = default;

		explicit IllegalJsonSegment(char const* Message)
			: exception{Message}{}

		IllegalJsonSegment(char const* Message, const int i)
			: exception{Message, i}{}

		explicit IllegalJsonSegment(exception const& Other)
			: exception{Other}{}
	};

	Integer parseInt(const std::string_view str, const int base = 10){
		Integer val{};
		std::from_chars(str.data(), str.data() + str.size(), val, base);
		return val;
	}

	Float parseFloat(const std::string_view str){
		Float val{};
		std::from_chars(str.data(), str.data() + str.size(), val);
		return val;
	}

	export enum struct jval_tag : std::size_t{
		null,
		arithmetic_int,
		arithmetic_float,
		boolean,
		string,
		array,
		object
	};



	static_assert(std::same_as<std::underlying_type_t<jval_tag>, std::size_t>);

	template <jval_tag Tag>
	struct TagBase : std::integral_constant<std::underlying_type_t<jval_tag>, std::to_underlying(Tag)>{
		static constexpr auto tag = Tag;
		static constexpr auto index = std::to_underlying(tag);
	};

	export
	template <typename T>
	struct json_type_to_index : TagBase<jval_tag::null>{};

	template <std::integral T>
	struct json_type_to_index<T> : TagBase<jval_tag::arithmetic_int>{};

	template <std::floating_point T>
	struct json_type_to_index<T> : TagBase<jval_tag::arithmetic_float>{};

	template <>
	struct json_type_to_index<bool> : TagBase<jval_tag::boolean>{};

	template <>
	struct json_type_to_index<Array> : TagBase<jval_tag::array>{};

	template <>
	struct json_type_to_index<Object> : TagBase<jval_tag::object>{};;

	template <>
	struct json_type_to_index<std::string> : TagBase<jval_tag::string>{};

	template <>
	struct json_type_to_index<std::string_view> : TagBase<jval_tag::string>{};

	export template <typename T>
	constexpr jval_tag jval_tag_of = json_type_to_index<T>::tag;

	export template <typename T>
	constexpr std::size_t jval_index_of = json_type_to_index<T>::index;

	export using enum jval_tag;

	export
	class json_value : type_to_index<std::tuple<std::nullptr_t, Integer, Float, bool, std::string, Array, Object>>{
	private:
		using VariantTypeTuple = args_type;
		using VariantType = tuple_to_variant_t<VariantTypeTuple>;
		VariantType data{};

	public:
		constexpr json_value() = default;

		template <typename T>
			requires is_type_valid<T> && !std::is_arithmetic_v<T>
		explicit json_value(T&& val) : data{std::forward<T>(val)}{}

		template <typename T>
			requires std::is_arithmetic_v<T> || std::same_as<T, bool>
		explicit json_value(const T val) : data{static_cast<json_scalar_t<T>>(val)}{}

		explicit json_value(const std::string_view val) : data{std::string{val}}{}
		json_value(parse_string_t, std::string_view json_str);

		void set(const std::string_view str){
			data = static_cast<std::string>(str);
		}

		template <typename T>
			requires is_type_valid<std::decay_t<T>> && !std::is_arithmetic_v<T>
		void set(T&& val){
			data = std::forward<T>(val);
		}

		template <typename T>
			requires is_type_valid<json_scalar_t<T>> && std::is_arithmetic_v<T>
		void set(const T val){
			data = static_cast<json_scalar_t<T>>(val);
		}

		template <jval_tag tag>
		[[nodiscard]] constexpr bool is() const noexcept{
			return get_tag() == tag;
		}

		template <typename T>
			requires is_type_valid<T>
		[[nodiscard]] constexpr bool is() const noexcept{
			return get_tag() == index_of<T>;
		}

		void set(const bool val){
			data = val;
		}

		void set(const std::nullptr_t){
			data = nullptr;
		}

		[[nodiscard]] constexpr auto get_tag() const noexcept{
			return jval_tag{data.index()};
		}

		[[nodiscard]] constexpr auto get_index() const noexcept{
			return data.index();
		}

		json_value& operator[](const std::string_view key){
			if(!is<object>()){
				throw std::bad_variant_access{};
			}

			return as_obj().at(key);
		}

		const json_value& operator[](const std::string_view key) const{
			if(!is<object>()){
				throw std::bad_variant_access{};
			}

			return as_obj().at(key);
		}

		template <typename T>
			requires (std::is_arithmetic_v<T> && is_type_valid<json_scalar_t<T>>)
		[[nodiscard]] constexpr json_scalar_t<T> get_or(const T def) const noexcept{
			auto* val = try_get<json_scalar_t<T>>();
			if(val)return *val;
			return def;
		}

		template <typename T>
			requires is_type_valid<T> || (std::is_arithmetic_v<T> && is_type_valid<json_scalar_t<T>>)
		[[nodiscard]] constexpr decltype(auto) as(){
			if constexpr(std::is_arithmetic_v<T>){
				return static_cast<T>(std::get<json_scalar_t<T>>(data));
			} else{
				return std::get<T>(data);
			}
		}

		template <typename T>
			requires is_type_valid<T> || (std::is_arithmetic_v<T> && is_type_valid<json_scalar_t<T>>)
		[[nodiscard]] constexpr decltype(auto) as() const{
			if constexpr(std::is_arithmetic_v<T>){
				return static_cast<T>(std::get<json_scalar_t<T>>(data));
			} else{
				return std::get<T>(data);
			}
		}

		template <jval_tag tag>
		[[nodiscard]] constexpr decltype(auto) as(){
			return std::get<std::to_underlying(tag)>(data);
		}

		template <jval_tag tag>
		[[nodiscard]] constexpr decltype(auto) as() const{
			return std::get<std::to_underlying(tag)>(data);
		}

		Object& as_obj(){
			if(get_tag() != object){
				set(Object{});
			}

			return std::get<Object>(data);
		}

		[[nodiscard]] const Object& as_obj() const{
			return std::get<Object>(data);
		}

		decltype(auto) as_arr(){
			if(get_tag() != array){
				set(Array{});
			}

			return std::get<Array>(data);
		}

		[[nodiscard]] decltype(auto) as_arr() const{
			return std::get<Array>(data);
		}

		//TODO move these to something adapter like
		template <typename T>
			requires (std::same_as<std::decay_t<T>, json_value> || is_type_valid<T>)
		void append(const char* name, T&& val){
			this->append(std::string{name}, std::forward<T>(val));
		}

		template <typename T>
			requires (std::same_as<std::decay_t<T>, json_value> || is_type_valid<T>)
		void append(const std::string_view name, T&& val){
			if(get_tag() != object){
				throw std::bad_variant_access{};
			}

			if constexpr(std::same_as<T, json_value>){
				as<object>().insert_or_assign(name, std::forward<T>(val));
			} else{
				as<object>().insert_or_assign(name, json_value{std::forward<T>(val)});
			}
		}

		template <typename T>
			requires (std::same_as<std::decay_t<T>, json_value> || is_type_valid<T>)
		void append(std::string&& name, T&& val){
			if(get_tag() != object){
				throw std::bad_variant_access{};
			}

			if constexpr(std::same_as<T, json_value>){
				as<object>().insert_or_assign(std::move(name), std::forward<T>(val));
			} else{
				as<object>().insert_or_assign(std::move(name), json_value{std::forward<T>(val)});
			}
		}

		template <typename T>
			requires (std::same_as<T, json_value> || is_type_valid<T>)
		void push_back(T&& val){
			if(get_tag() != array){
				throw std::bad_variant_access{};
			}

			if constexpr(std::same_as<T, json_value>){
				as<array>().push_back(std::forward<T>(val));
			} else{
				as<array>().emplace_back(std::forward<T>(val));
			}
		}

		friend bool operator==(const json_value& lhs, const json_value& rhs) = default;

		template <typename T>
			requires is_type_valid<T>
		json_value& operator=(T&& val){
			this->set(std::forward<T>(val));
			return *this;
		}


		template <typename T>
			requires is_type_valid<T>
		[[nodiscard]] constexpr std::add_pointer_t<T> try_get() noexcept{
			return std::get_if<T>(&data);
		}

		template <typename T>
			requires is_type_valid<T>
		[[nodiscard]] constexpr std::add_pointer_t<const T> try_get() const noexcept{
			return std::get_if<T>(&data);
		}

		template <jval_tag tag>
		[[nodiscard]] constexpr std::add_pointer_t<arg_at<std::to_underlying(tag)>> try_get() noexcept{
			return std::get_if<std::to_underlying(tag)>(&data);
		}

		template <jval_tag tag>
		[[nodiscard]] constexpr std::add_pointer_t<const arg_at<std::to_underlying(tag)>> try_get() const noexcept{
			return std::get_if<std::to_underlying(tag)>(&data);
		}

		//TODO better format
		void print(std::ostream& os, const bool flat = false, const bool noSpace = false, const unsigned padSize = 1,
			const unsigned depth = 1) const{
			const std::string pad(flat ? 0 : depth * padSize, ' ');
			const std::string_view endRow =
				flat
					? noSpace
						  ? ""
						  : " "
					: "\n";

			switch(get_tag()){
				case arithmetic_int :{
					os << std::to_string(as<arithmetic_int>());
					break;
				}

				case arithmetic_float :{
					os << std::to_string(as<arithmetic_float>());
					break;
				}

				case boolean :{
					os << (as<boolean>() ? "true" : "false");
					break;
				}

				case string :{
					os << '"' << as<string>() << '"';
					break;
				}

				case array :{
					os << '[' << endRow;
					const auto data = as<array>();

					if(!data.empty()){
						int index = 0;
						for(; index < data.size() - 1; ++index){
							os << pad;
							data[index].print(os, flat, noSpace, padSize, depth + 1);
							os << ',' << endRow;
						}

						os << pad;
						data[index].print(os, flat, noSpace, padSize, depth + 1);
						os << endRow;
					}

					os << std::string_view(pad).substr(0, pad.size() - 4) << ']';
					break;
				}

				case object :{
					os << '{' << endRow;

					const auto data = as<object>();

					int count = 0;
					for(const auto& [k, v] : data){
						os << pad << '"' << k << '"' << (noSpace ? ":" : " : ");
						v.print(os, flat, noSpace, padSize, depth + 1);
						count++;
						if(count == data.size()) break;
						os << ',' << endRow;
					}

					os << endRow;
					os << std::string_view(pad).substr(0, pad.size() - 4) << '}';
					break;
				}

				default : os << "null";
			}
		}

		friend std::ostream& operator<<(std::ostream& os, const json_value& obj){
			obj.print(os);

			return os;
		}

		[[nodiscard]] constexpr bool is_arithmetic() const noexcept{
			return
				is<arithmetic_int>() ||
				is<arithmetic_float>();
		}

		[[nodiscard]] constexpr bool is_scalar() const noexcept{
			return
				is_arithmetic() ||
				is<boolean>() ||
				is<null>();
		}

		template <typename T>
			requires (std::is_arithmetic_v<T>)
		[[nodiscard]] constexpr T as_arithmetic(const T def = T{}) const noexcept{
			if(is<Integer>()){
				return static_cast<T>(as<Integer>());
			}

			if(is<Float>()){
				return static_cast<T>(as<Float>());
			}

			return def;
		}

		template <typename T>
			requires (std::is_arithmetic_v<T> || std::same_as<T, bool> || std::same_as<T, std::nullptr_t>)
		[[nodiscard]] constexpr T as_scalar(const T def = T{}) const noexcept{
			if constexpr (std::is_arithmetic_v<T>){
				if(is<bool>()){
					return static_cast<T>(as<bool>());
				}

				return this->as_arithmetic<T>(def);
			}

			if constexpr (std::same_as<T, bool>){
				if(is<null>()){
					return false;
				}

				if(is<bool>()){
					return as<bool>();
				}

				return static_cast<bool>(this->as_arithmetic<Integer>(static_cast<Integer>(def)));
			}

			if constexpr (std::same_as<T, std::nullptr_t>){
				return nullptr;
			}

			std::unreachable();
		}
	};

	export
	constexpr json_value null_jval{};
}

namespace ext::json{
	constexpr std::string_view trim(const std::string_view view){
		constexpr std::string_view Empty = " \t\n\r\f\v";
		const auto first = view.find_first_not_of(Empty);

		if(first == std::string_view::npos){
			return {};
		}

		const auto last = view.find_last_not_of(Empty);
		return view.substr(first, last - first + 1);
	}

	json_value parseBasicValue(const std::string_view view){
		if(view.empty()){
			return json_value(nullptr);
		}

		const auto frontChar = view.front();

		switch(frontChar){
			case '"' :{
				if(view.back() != '"' || view.size() == 1)
					throw IllegalJsonSegment{
						"Losing '\"' At Json String Back"
					};

				return json_value{view.substr(1, view.size() - 2)};
			}

			case 't' : return json_value(true);
			case 'f' : return json_value(false);
			case 'n' : return json_value(nullptr);
			default : break;
		}

		if(view.find_first_of(".fFeEiInN") != std::string_view::npos){
			return json_value(parseFloat(view));
		}

		if(frontChar == '0'){
			if(view.size() >= 2){
				switch(view[1]){
					case 'x' : return json_value(parseInt(view.substr(2), 16));
					case 'b' : return json_value(parseInt(view.substr(2), 2));
					default : return json_value(parseInt(view.substr(1), 8));
				}
			} else{
				return json_value(static_cast<Integer>(0));
			}
		} else{
			return json_value(parseInt(view));
		}
	}

	json_value parseJsonFromString(std::string_view text){
		if(text.empty())return {};
		static constexpr auto Invalid = std::string_view::npos;

		bool escapeTheNext{false};
		bool parsingString{false};

		struct LayerData{
			std::size_t layerBegin{};
			json_value value{};

			std::size_t layerLastSplit{};
			json_value* last{};

			void processData(const std::size_t index, const std::string_view text){
				if(layerLastSplit != Invalid){
					const auto lastCode = trim(text.substr(layerLastSplit + 1, index - layerLastSplit - 1));

					if(lastCode.empty())return;

					if(value.is<object>()){
						last->operator=(parseBasicValue(lastCode));
					}else{
						value.as_arr().push_back(parseBasicValue(lastCode));
					}

				}

				layerLastSplit = index;
			}
		};

		std::stack<LayerData, std::list<LayerData>> layers{};

		for(const auto& [index, character] : text | std::ranges::views::enumerate){
			if(escapeTheNext){
				escapeTheNext = false;
				continue;
			}

			if(character == '\\'){
				escapeTheNext = true;
				continue;
			}

			if(character == '"'){
				if(!parsingString){
					parsingString = true;
				} else{
					parsingString = false;
				}
			}

			if(parsingString) continue;

			switch(character){
				case ':' :{
					auto& [begin, layer, split, last] = layers.top();
					auto lastKey = trim(text.substr(split + 1, index - split - 1));
					last = &layer.as_obj().insert_or_assign(lastKey.substr(1, lastKey.size() - 2), json_value{nullptr}).first->second;

					split = index;

					break;
				}

				case '[' :{
					layers.emplace(index, json_value{Array{}}, index);
					break;
				}

				case '{' :{
					layers.emplace(index, json_value{Object{}}, index);
					break;
				}

				case ']' :
				case '}' :{
					auto lastLayer = std::move(layers.top());
					layers.pop();

					lastLayer.processData(index, text);

					if(layers.empty()) return std::move(lastLayer.value);

					auto& [pos, ly, split, last] = layers.top();

					if(ly.is<object>()){
						last->operator=(std::move(lastLayer.value));
					}

					if(ly.is<array>()){
						ly.as_arr().push_back(std::move(lastLayer.value));
					}

					split = Invalid;

					break;
				}

				case ',' :{
					layers.top().processData(index, text);
				}
				default : break;
			}
		}

		std::unreachable();
	}

	export
	[[nodiscard]] json_value parse(const std::string_view view){
		return parseJsonFromString(view);
	}
}

export
template <>
struct ::std::formatter<ext::json::json_value>{
	constexpr auto parse(std::format_parse_context& context) const{
		return context.begin();
	}

	bool flat{false};
	bool noSpace{false};
	int pad{2};

	constexpr auto parse(std::format_parse_context& context){
		auto it = context.begin();
		if(it == context.end()) return it;

		if(*it == 'n'){
			noSpace = true;
			++it;
		}

		if(*it == 'f'){
			flat = true;
			++it;
		}

		if(*it >= '0' && *it <= '9'){
			pad = *it - '0';
			++it;
		}

		if(it != context.end() && *it != '}') throw std::format_error("Invalid format");

		return it;
	}

	auto format(const ext::json::json_value& json, auto& context) const{
		std::ostringstream ss{};
		json.print(ss, flat, noSpace);

		return std::format_to(context.out(), "{}", ss.str());
	}
};

module : private;

ext::json::json_value::json_value(parse_string_t, const std::string_view json_str){
	this->operator=(parse(json_str));
}

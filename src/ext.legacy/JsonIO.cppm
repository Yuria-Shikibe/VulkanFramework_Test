export module ext.json.io;

export import ext.json;
import ext.meta_programming;
import ext.owner;
import ext.Base64;
import ext.StaticReflection;
import ext.heterogeneous;

import std;

namespace ext::json{
	export
	template <typename T>
		requires !std::is_pointer_v<T>
	struct json_serializer{
		static void write(json_value& jsonValue, const T& data) = delete;

		static void read(const json_value& jsonValue, T& data) = delete;
	};

	export
	template <typename T>
	inline constexpr bool is_json_directly_serializable_v = requires (json_value& j, T& t){
		json_serializer<T>::write(j, t);
		json_serializer<T>::read(j, t);
	};
}

template<typename T = void>
struct TriggerFailure
{
	template<typename> static constexpr auto value = false;
	static_assert(value<T>, "Json Serialization Not Support This Type");
};

export namespace ext::json{
	struct JsonSerializationException final : std::exception{
		JsonSerializationException() = default;

		explicit JsonSerializationException(char const* Message)
			: exception{Message}{}

		JsonSerializationException(char const* Message, const int i)
			: exception{Message, i}{}

		explicit JsonSerializationException(exception const& Other)
			: exception{Other}{}
	};

	struct json_dynamic_serializable{
		virtual ~json_dynamic_serializable() = default;

		virtual void write(json_value& jval) const = 0;
		virtual void read(const json_value& jval) = 0;

		void save_type(json_value& jval) const {
			auto& map = jval.as_obj();
			const std::string_view name = reflect::classNames_RTTI().at(get_type_index());
			map.insert_or_assign(keys::Typename, json_value{name});
		}

		template <typename T>
		[[nodiscard]] static owner<T*> generate(const json_value& jval) noexcept(std::same_as<void, T>){
			if(jval.is<object>()){
				auto& map = jval.as_obj();
				if(const auto itr = map.find(keys::Typename); itr != map.end() && itr->second.is<string>()){
					return static_cast<T*>(reflect::tryConstruct(itr->second.as<string>()));
				}
			}

			return nullptr;
		}

		template <typename T>
		[[nodiscard]] static owner<T*> generate_unchecked(const json_value& jval){
			return static_cast<T*>(reflect::tryConstruct(jval.as_obj().at(keys::Typename).as<string>()));
		}

		[[nodiscard]] std::type_index get_type_index() const noexcept{
			return typeid(*this);
		}
	};

	template <typename T = void>
	owner<T*> getObjectFrom(const json_value& jval){
		if(jval.is<object>()){
			const auto& map = jval.as_obj();
			if(const auto itr = map.find(keys::Typename); itr != map.end()){
				return reflect::tryConstruct<T>(itr->second.as<string>());
			}
		}
 		return nullptr;
	}


	/**
	 * @brief
	 * @tparam T TypeName
	 * @tparam isWriting true -> write | false -> read
	 */
	template <typename T, bool isWriting>
	struct JsonFieldIOCallable{
	private:
		using FieldHint = reflect::Field<nullptr>;

	public:
		using ClassField = reflect::ClassField<T>;
		using PassType = typename ConstConditional<isWriting, T&>::type;
		using DataPassType = typename ConstConditional<!isWriting, json_value&>::type;

		template <std::size_t I>
		constexpr static void with(PassType val, DataPassType src){
			if constexpr (isWriting){
				JsonFieldIOCallable::write<I>(val, src);
			}else{
				JsonFieldIOCallable::read<I>(val, src);
			}
		}

		template <std::size_t I>
		constexpr static void read(PassType val, DataPassType src);

		template <std::size_t I>
		constexpr static void write(PassType val, DataPassType src);
	};

	template <typename T>
	void writeToJson(json_value& jval, const T& val){
		using RawT = std::decay_t<T>;
		if constexpr (is_json_directly_serializable_v<RawT>){
			json_serializer<RawT>::write(jval, val);
		}else if constexpr (reflect::ClassField<RawT>::defined){
			reflect::ClassField<RawT>::template fieldEach<JsonFieldIOCallable<RawT, true>>(val, jval);
		}else{
			(void)TriggerFailure<std::decay_t<T>>{};
		}
	}

	template <typename T>
	void readFromJson(const json_value& jval, T& val){
		using RawT = std::decay_t<T>;

		if constexpr (is_json_directly_serializable_v<RawT>){
			json_serializer<RawT>::read(jval, val);
		}else if constexpr (reflect::ClassField<RawT>::defined){
			reflect::ClassField<RawT>::template fieldEach<JsonFieldIOCallable<RawT, false>>(val, jval);
		}else{
			(void)TriggerFailure{};
		}
	}

	template <typename T>
	json_value getJsonOf(T&& val){
		json_value jval{};
		json::writeToJson(jval, std::forward<T>(val));

		return jval;
	}

	template <typename T>
	T& getValueTo(T& val, const json_value& jval){
		json::readFromJson(jval, val);
		return val;
	}

	template <typename T>
		requires std::is_default_constructible_v<T>
	T getValueFrom(const json_value& jval){
		T t{};
		json::readFromJson(jval, t);
		return t;
	}

	void append(json_value& jval, const std::string_view key, json_value&& val){
		jval.as_obj().insert_or_assign(key, std::move(val));
	}

	template <typename T>
	void append(json_value& jval, const std::string_view key, const T& val){
		jval.as_obj().insert_or_assign(key, json::getJsonOf(val));
	}

	template <typename T>
	void push_back(json_value& jval, const T& val){
		jval.as_arr().push_back(json::getJsonOf(val));
	}

	template <typename T>
	void read(const json_value& jval, const std::string_view key, T& val){
		json::getValueTo(val, jval.as_obj().at(key));
	}

	template <typename T>
		requires std::is_copy_assignable_v<T>
	void read(const json_value& jval, const std::string_view key, T& val, const T& defValue){
		auto& map = jval.as_obj();
		if(const auto itr = map.find(key); itr != map.end()){
			json::getValueTo(val, itr->second);
		}else{
			val = defValue;
		}
	}
}

export namespace ext::json{
	template <typename T>
	constexpr bool jsonSerializable = requires(T& val){
		json::getJsonOf(val);
		json::getValueTo(val, json_value{});
	};

	template <typename T, bool isWriting>
	template <std::size_t I>
	constexpr void JsonFieldIOCallable<T, isWriting>::read(PassType val, DataPassType src){
		using Field = typename ClassField::template FieldAt<I>;
		using FieldClassInfo = typename Field::ClassInfo;

		if(src.get_tag() != object)return; //TODO throw maybe?

		if constexpr(Field::getSrlType != reflect::SrlType::disable){
			auto& member = val.*Field::mptr;
			constexpr std::string_view fieldName = Field::getName;

			constexpr auto srlType = ext::conditional_v<
				FieldClassInfo::srlType == reflect::SrlType::depends,
				Field::getSrlType, FieldClassInfo::srlType>;

			const auto itr = src.as_obj().find(fieldName);
			if(itr == src.as_obj().end())return;
			const json_value& jval = itr->second;

			if constexpr (srlType == reflect::SrlType::json){
				json::getValueTo(member, jval);
			}else if constexpr (srlType == reflect::SrlType::binary_all){
				(void)TriggerFailure<T>{};
				//Core::IO::fromByte(member, ext::base64::decode<std::vector<char>>(jval.as<ext::json::string>()));
			}else if constexpr (srlType == reflect::SrlType::binary_byMember){
				//TODO binary IO support
				(void)TriggerFailure<T>{};
			}else{
				(void)TriggerFailure<T>{};
			}
		}
	}

	template <typename T, bool write>
	template <std::size_t I>
	constexpr void JsonFieldIOCallable<T, write>::write(PassType val, DataPassType src){
		using Field = typename ClassField::template FieldAt<I>;
		using FieldClassInfo = typename Field::ClassInfo;

		if constexpr(Field::getSrlType != reflect::SrlType::disable){
			constexpr std::string_view fieldName = Field::getName;
			constexpr auto srlType = ext::conditional_v<
				FieldClassInfo::srlType == reflect::SrlType::depends,
				Field::getSrlType, FieldClassInfo::srlType>;

			(void)src.as_obj(); //TODO is this really good?

			if constexpr (srlType == reflect::SrlType::json){
				src.append(fieldName, json::getJsonOf(std::forward<PassType>(val).*Field::mptr));
			}else if constexpr (srlType == reflect::SrlType::binary_all){
				(void)TriggerFailure<T>{};
				// ext::json::JsonValue value{};
				//
				// auto data = Core::IO::toByte(val);
				// value.setData<std::string>(ext::base64::encode<std::string>(data));
				// src.append(fieldName, std::move(value));
			}else if constexpr (srlType == reflect::SrlType::binary_byMember){
				//TODO binary IO support
				(void)TriggerFailure<T>{};
			}else{
				(void)TriggerFailure<T>{};
			}
		}
	}


	template <>
	struct json::json_serializer<std::string_view>{
		static void write(json_value& jsonValue, const std::string_view data){
			jsonValue.set<std::string>(static_cast<std::string>(data));
		}

		static void read(const json_value& jsonValue, std::string_view& data) = delete;
	};

	template <>
	struct json::json_serializer<std::string>{
		static void write(json_value& jsonValue, const std::string& data){
			jsonValue.set(data);
		}

		static void read(const json_value& jsonValue, std::string& data){
			data = jsonValue.as<std::string>();
		}
	};

	template <std::ranges::sized_range Cont>
		requires !std::is_pointer_v<std::ranges::range_value_t<Cont>> && ext::json::jsonSerializable<
			std::ranges::range_value_t<Cont>>
	struct JsonSrlContBase_vector{
		static void write(json_value& jsonValue, const Cont& data){
			auto& val = jsonValue.as_arr();
			val.resize(data.size());
			std::ranges::transform(data, val.begin(), []<typename T>(T&& t){
				return json::getJsonOf(std::forward<T>(t));
			});
		}

		static void read(const json_value& jsonValue, Cont& data){
			if(auto* ptr = jsonValue.try_get<array>()){
				data.resize(ptr->size());

				for(auto&& [index, element] : data | std::ranges::views::enumerate){
					json::getValueTo(element, ptr->at(index));
				}
			}
		}
	};

	template <typename K, typename V,
	          class Hasher = std::hash<K>, class Keyeq = std::equal_to<K>,
	          class Alloc = std::allocator<std::pair<const K, V>>>
		requires
		!std::is_pointer_v<K> && !std::is_pointer_v<V> &&
		std::is_default_constructible_v<K> && std::is_default_constructible_v<V>
	struct JsonSrlContBase_unordered_map{
		//OPTM using array instead of object to be the KV in json?
		static void write(json_value& jsonValue, const std::unordered_map<K, V, Hasher, Keyeq, Alloc>& data){
			auto& val = jsonValue.as_arr();
			val.reserve(data.size());

			for(auto& [k, v] : data){
				json_value jval{};
				jval.as_obj();
				jval.append(keys::Key, json::getJsonOf(k));
				jval.append(keys::Value, json::getJsonOf(v));
				val.push_back(std::move(jval));
			}
		}

		static void read(const json_value& jsonValue, std::unordered_map<K, V>& data){
			if(auto* ptr = jsonValue.try_get<array>()){
				data.reserve(ptr->size());

				for(const auto& jval : *ptr){
					auto& pair = jval.as_obj();

					K k{};
					V v{};
					json::getValueTo(k, pair.at(keys::Key));
					json::getValueTo(v, pair.at(keys::Value));

					data.insert_or_assign(std::move(k), std::move(v));
				}
			}
		}
	};

	template <typename V, bool overwirteOnly = false>
		requires !std::is_pointer_v<V> && std::is_default_constructible_v<V>
	struct JsonSrlContBase_string_map{
		//OPTM using array instead of object to be the KV in json?
		static void write(json_value& jsonValue, const string_hash_map<V>& data){
			auto& val = jsonValue.as_obj();
			val.reserve(data.size());

			for(auto& [k, v] : data){
				val.insert_or_assign(k, json::getJsonOf(v));
			}
		}

		static void read(const json_value& jsonValue, string_hash_map<V>& data){
			if(auto* ptr = jsonValue.try_get<object>()){
				data.reserve(ptr->size());

				for(const auto& [k, v] : *ptr){
					if constexpr (overwirteOnly){
						V* d = data.try_find(k);
						if(d){
							json::getValueTo(*d, v);
						}
					}else{
						data.insert_or_assign(std::string(k), json::getValueFrom<V>(v));
					}

				}
			}
		}
	};
}
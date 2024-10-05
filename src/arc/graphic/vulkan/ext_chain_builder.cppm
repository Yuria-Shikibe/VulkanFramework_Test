export module Core.Vulkan.ExtChainBuilder;

import std;

export namespace Core::Vulkan{
	using u32 = std::uint32_t;
	using szt = std::size_t;

	template <typename St>
	struct struct_traits;
	template <template<typename>typename ST, typename T>
	struct apply{
		template <typename O>
		struct impl{
			using type = ST<void>;
		};
		template <typename O>
			requires(requires{ typename O::next; })
		struct impl<O>{
			template <typename K>
			struct iimpl{
				using type = ST<K>;
			};
			template <template<template<typename>typename, typename...>typename OT,
			          template<typename>typename Tp,
			          typename... Ts>
			struct iimpl<OT<Tp, Ts...>>{
				using type = OT<Tp, Ts...>;
			};

			using type = typename iimpl<typename O::next>::type;
		};

		using type = typename impl<typename struct_traits<T>>::type;
	};
	template <template<typename>typename ST, typename T>
	using apply_t = typename apply<ST, T>::type;

	template <typename St>
	struct structure
		: apply_t<structure, St>{
		using traits = struct_traits<St>;
		using base = apply_t<structure, St>;

		using element_type = St;
		using value_type = St;

		template <typename T>
		explicit structure(T&& h) noexcept
			requires(!::std::is_same_v<::std::remove_cvref_t<T>, element_type>)
			: base{h}
			, value{traits::value, base::template put<void>()}{
			if constexpr(requires{ traits::init(::std::declval<T&>(), ::std::declval<St&>()); }) traits::init(h, value);
			else if constexpr(requires{ { traits::init(::std::declval<T&>()) } -> ::std::same_as<value_type>; }){
				value = traits::init(h);
				value.sType = traits::value;
				value.pNext = base::template put<void>();
			}
		}

		structure(element_type const& v)
			: base{v.pNext}
			, value{v}{
			value.sType = traits::value;
			value.pNext = base::template put<void>();
		}

		template <typename T>
		explicit structure(T const* v)
			requires::std::is_same_v<T, void>
			: base{static_cast<element_type const*>(v)->pNext}
			, value{*static_cast<element_type const*>(v)}{}

		structure(structure const&) = delete;

		structure& operator=(structure const&) = delete;

		structure(structure&&) = default;

		structure& operator=(structure&&) = default;

		template <typename T = element_type>
		decltype(auto) get() const noexcept{
			if constexpr(::std::is_same_v<element_type, T>) return (value);
			else return base::template get<T>();
		}

		template <typename T = element_type>
		decltype(auto) get() noexcept{
			if constexpr(::std::is_same_v<element_type, T>) return (value);
			else return base::template get<T>();
		}

		template <typename T = element_type>
		auto put(){
			if constexpr(::std::is_void_v<T>){
				if constexpr(requires(element_type const& t){ traits::enable(t); }) if(!traits::enable(value)) return
					base::template put<void>();
				return (void*)&value;
			} else if constexpr(::std::is_same_v<T, element_type>) return &value;
			else{
				if constexpr(!::std::is_null_pointer_v<decltype(base::template put<T>())>) return base::template put<T>();
				else static_assert(::std::is_void_v<T> ? false : false, "Not vaild put.");
			}
		}

		template <typename T = element_type>
		auto put() const{
			if constexpr(::std::is_void_v<T>){
				if constexpr(requires(element_type const& t){ traits::enable(t); }) if(!traits::enable(value)) return
					base::template put<void>();
				return (void*)&value;
			} else if constexpr(::std::is_same_v<T, element_type>) return &value;
			else{
				if constexpr(!::std::is_null_pointer_v<decltype(base::template put<T>())>) return base::template put<T>();
				else static_assert(::std::is_void_v<T> ? false : false, "Not vaild put.");
			}
		}

	private:
		value_type value;
	};
	template <>
	struct structure<void>{
		constexpr structure(auto&&){};

		template <typename = void>
		static constexpr ::std::nullptr_t put() noexcept{ return nullptr; }

		template <typename = void>
		static constexpr ::std::nullptr_t get() noexcept{ return {}; }
	};

	// template <typename St>
	// struct range_structure
	// 	: apply_t<range_structure, St>{
	// 	using traits = struct_traits<St>;
	// 	using base = apply_t<range_structure, St>;
	// 	using element_type = St;
	// 	using value_type = ::std::vector<St>;
	//
	// 	template <typename T>
	// 	explicit(true) range_structure(T&& desc)
	// 		: base{desc}{
	// 		if constexpr(
	// 			requires{
	// 				traits::init(
	// 					::std::declval<T&>(),
	// 					::std::declval<u32&>(),
	// 					::std::declval<element_type*>());
	// 			}){
	// 			u32 size{0u};
	// 			traits::init(desc, size, nullptr);
	//
	// 			values.reserve(size);
	// 			for(auto i{0u}; i < size; i++) values.emplace_back(traits::value, base::put(i));
	// 			traits::init(desc, size, values.data());
	// 		} else if constexpr(
	// 			requires{
	// 				{ traits::init(::std::declval<T&>(), ::std::declval<element_type&>()) } -> ::std::integral;
	// 			}){
	// 			u32 result;
	// 			for(element_type value; (result = (u32)traits::init(desc, value)) < 2u;) if(result == 1u) values.emplace_back(
	// 				::std::move(value));
	// 		}
	// 	}
	//
	// 	range_structure(range_structure const&) = delete;
	//
	// 	range_structure& operator=(range_structure const&) = delete;
	//
	// 	range_structure(range_structure&&) = default;
	//
	// 	range_structure& operator=(range_structure&&) = default;
	//
	// 	template <typename T = element_type>
	// 	inline constexpr structure<element_type> at(szt index = 0u){
	// 		if constexpr(::std::is_same_v<element_type, T>){
	// 			GSW_ASSERT(index < values.size());
	// 			return {values.at(index)};
	// 		} else return base::template at<T>(index);
	// 	}
	//
	// 	template <typename T = element_type>
	// 	inline constexpr structure<element_type> at(szt index = 0u) const{
	// 		if constexpr(::std::is_same_v<element_type, T>){
	// 			GSW_ASSERT(index < values.size());
	// 			return {values.at(index)};
	// 		} else return base::template at<T>(index);
	// 	}
	//
	// 	template <typename T = element_type>
	// 	inline constexpr auto put(szt index = 0u){
	// 		auto& value{values.at(index)};
	// 		if constexpr(::std::is_void_v<T>){
	// 			if constexpr(requires(element_type const& t){ traits::enable(t); }) if(!traits::enable(value)) return
	// 				base::template put<void>(index);
	// 			return &value;
	// 		} else if constexpr(::std::is_void_v<T> || ::std::is_same_v<T, element_type>) return &value;
	// 		else{
	// 			if constexpr(!::std::is_null_pointer_v<decltype(base::template put<T>())>) return base::template put<T>(index);
	// 			else static_assert(::std::is_void_v<T> ? false : false, "Not vaild put.");
	// 		}
	// 	}
	//
	// 	template <typename T = element_type>
	// 	inline constexpr auto put(szt index = 0u) const{
	// 		auto& value{values.at(index)};
	// 		if constexpr(::std::is_void_v<T>){
	// 			if constexpr(requires(element_type const& t){ traits::enable(t); }) if(!traits::enable(value)) return
	// 				base::template put<void>(index);
	// 			return &value;
	// 		} else if constexpr(::std::is_void_v<T> || ::std::is_same_v<T, element_type>) return &value;
	// 		else{
	// 			if constexpr(!::std::is_null_pointer_v<decltype(base::template put<T>())>) return base::template put<T>(index);
	// 			else static_assert(::std::is_void_v<T> ? false : false, "Not vaild put.");
	// 		}
	// 	}
	//
	// 	auto size() const{ return values.size(); }
	//
	// 	inline constexpr auto begin(){ return values.begin(); }
	//
	// 	inline constexpr auto end(){ return values.end(); }
	//
	// 	inline constexpr auto begin() const{ return values.begin(); }
	//
	// 	inline constexpr auto end() const{ return values.end(); }
	//
	// private:
	// 	value_type values;
	// };
	// template <>
	// struct range_structure<void>{
	// 	constexpr range_structure(auto&&){}
	//
	// 	template <typename = void>
	// 	inline static constexpr ::std::nullptr_t at(szt) noexcept{ return nullptr; }
	//
	// 	template <typename = void>
	// 	inline static constexpr ::std::nullptr_t put(szt) noexcept{ return nullptr; }
	// };
	//
	// template <template<typename>typename ST, typename Fst, typename... Rst>
	// struct optional{
	// 	template <typename T, typename S>
	// 	struct biggest{
	// 		static constexpr auto value{sizeof(T) < sizeof(S)};
	// 	};
	//
	// 	// select biggest one.
	// 	using biggest_struct = typename most_in<biggest, ST<Fst>, ST<Rst>...>::type;
	//
	// 	using structs = ::std::tuple<Fst, Rst...>;
	//
	// 	static constexpr auto num_struct{sizeof...(Rst) + 1u};
	// 	static constexpr auto range{::std::ranges::range<ST<Fst>>};
	//
	// 	template <::std::uint8_t index = 0, typename T = void>
	// 	static constexpr ::std::uint8_t select(T&& desc){
	// 		if constexpr(index < num_struct){
	// 			if(struct_traits<element_at<index, structs>>::enable(desc)) return index;
	// 			else return select<index + 1>(desc);
	// 		} else return num_struct;
	// 	}
	//
	// 	template <::std::uint8_t index = 0, typename T = void>
	// 	void init(T&& desc){
	// 		if constexpr(index < num_struct){
	// 			if(which == index) new(storage) ST<element_at<index, structs>>{desc};
	// 			else this->template init<index + 1>(desc);
	// 		}
	// 	}
	//
	// 	template <szt index = 0>
	// 	void* put() noexcept{
	// 		if constexpr(index < num_struct){
	// 			if(which == index) return put<element_at<index, structs>>();
	// 			else return put<index + 1>();
	// 		} else return nullptr;
	// 	}
	//
	// 	template <szt index = 0>
	// 	void const* put() const noexcept{
	// 		if constexpr(index < num_struct){
	// 			if(which == index) return put<element_at<index, structs>>();
	// 			else return put<index + 1>();
	// 		} else return nullptr;
	// 	}
	//
	// 	template <szt index = 0>
	// 	void* put(szt idx = 0u) noexcept{
	// 		if constexpr(index < num_struct){
	// 			if(which == index) return put<element_at<index, structs>>(idx);
	// 			else return put<index + 1>();
	// 		} else return nullptr;
	// 	}
	//
	// 	template <szt index = 0>
	// 	void const* put(szt idx = 0u) const noexcept{
	// 		if constexpr(index < num_struct){
	// 			if(which == index) return put<element_at<index, structs>>(idx);
	// 			else return put<index + 1>();
	// 		} else return nullptr;
	// 	}
	//
	// 	template <typename T>
	// 		requires((presented_at<T, structs>) || ::std::is_void_v<T>)
	// 	auto put(szt idx = 0u) noexcept requires(range){
	// 		if constexpr(!::std::is_void_v<T>){
	// 			constexpr auto index = index_at<T, structs>;
	// 			if(which == index) return ::std::bit_cast<ST<T>*>(&storage[0])->template put<T>(idx);
	// 			else return static_cast<T*>(nullptr);
	// 		} else{
	// 			return this->template put<0>(idx);
	// 		}
	// 	}
	//
	// 	template <typename T>
	// 		requires((presented_at<T, structs>) || ::std::is_void_v<T>)
	// 	auto put() noexcept requires(!range){
	// 		if constexpr(!::std::is_void_v<T>){
	// 			constexpr auto index = index_at<T, structs>;
	// 			if(index == which) return ::std::bit_cast<ST<T>*>(&storage[0])->template put<T>();
	// 			else return static_cast<T*>(nullptr);
	// 		} else{
	// 			return this->template put<0>();
	// 		}
	// 	}
	//
	// 	template <typename T>
	// 		requires((presented_at<T, structs>) || ::std::is_void_v<T>)
	// 	auto put(szt idx = 0u) const noexcept requires(range){
	// 		if constexpr(!::std::is_void_v<T>){
	// 			constexpr auto index = index_at<T, structs>;
	// 			if(which == index) return ::std::bit_cast<ST<T> const*>(&storage[0])->template put<T>(idx);
	// 			else return static_cast<T const*>(nullptr);
	// 		} else{
	// 			return this->template put<0>();
	// 		}
	// 	}
	//
	// 	template <typename T>
	// 		requires((presented_at<T, structs>) || ::std::is_void_v<T>)
	// 	auto put() const noexcept requires(!range){
	// 		if constexpr(!::std::is_void_v<T>){
	// 			constexpr auto index = index_at<T, structs>;
	// 			if(index == which) return ::std::bit_cast<ST<T>*>(&storage[0])->template put<T>();
	// 			else return static_cast<T const*>(nullptr);
	// 		} else{
	// 			return this->template put<0>();
	// 		}
	// 	}
	//
	// 	template <presented_at<structs> T>
	// 	bool enabled() const{ return which == index_of<T, Fst, Rst...>; }
	//
	// 	bool enabled() const{ return which == num_struct; }
	//
	// 	template <typename T>
	// 	optional(T&& desc) noexcept
	// 		: which{select(desc)}{
	// 		init(desc);
	// 	}
	//
	// 	optional(optional const&) = delete;
	//
	// 	optional(optional&& other)
	// 		: which{other.which}{ ::std::memmove(storage, other.storage, sizeof(biggest_struct)); }
	//
	// 	// which is zero means not enabled.
	// 	const ::std::uint8_t which{0u};
	//
	// private:
	// 	::std::uint8_t storage[sizeof(biggest_struct)];
	// };
}

export module ext.meta_programming;

import std;

namespace ext{
	export template <typename ToVerify, typename ...Targets>
	constexpr bool is_any_of = (std::same_as<ToVerify, Targets> || ...);

	export
	template <typename T>
	struct to_signed : std::type_identity<std::make_signed_t<T>>{};

	template <std::floating_point T>
	struct to_signed<T> : std::type_identity<T>{};

	template <typename T>
		requires std::same_as<std::remove_cv_t<T>, bool>
	struct to_signed<T> : std::type_identity<void>{
		// ReSharper disable once CppStaticAssertFailure
		static_assert(false, "to signed applied to bool type");
	};

	export template <typename T>
	using to_signed_t = typename to_signed<T>::type;

	export
	template <typename T>
	struct function_traits;

	export
	template <typename Ret, typename... Args>
	struct function_traits<Ret(Args...)>{
		using return_type = Ret;
		using args_type = std::tuple<Args...>;
		static constexpr std::size_t args_count = std::tuple_size_v<args_type>;
		static constexpr bool is_single = args_count == 1;

		template <typename Func>
		static constexpr bool is_invocable = std::is_invocable_r_v<Ret, Func, Args...>;

		template <typename Func>
		static constexpr bool invocable_as_r_v(){
			return std::is_invocable_r_v<Ret, Func, Args...>;
		}

		template <typename Func>
		static constexpr bool invocable_as_v(){
			return std::is_invocable_v<Func, Args...>;
		}
	};

	export
	template <typename Ret, typename T, typename... Args>
	struct function_traits<Ret(T::*)(Args...)> : function_traits<Ret(T*, Args...)>{};

	// export
	// template <typename T>
	// struct normalized_function{
	// 	using type = std::remove_pointer_t<T>;
	// };
	//
	// template <typename T>
	// 	requires std::is_member_function_pointer_v<T>
	// struct normalized_function<T>{
	// 	using type = std::remove_pointer_t<T>;
	// };

#define VariantFunc(ext) export template<typename Ret, typename... Args> struct function_traits<Ret(Args...) ext> : function_traits<Ret(Args...)>{};
	VariantFunc(&);
	VariantFunc(&&);
	VariantFunc(const);
	VariantFunc(const &);
	VariantFunc(noexcept);
	VariantFunc(& noexcept);
	VariantFunc(&& noexcept);
	VariantFunc(const noexcept);
	VariantFunc(const& noexcept);

#define VariantMemberFunc(ext) export template<typename Ret, typename T, typename... Args> struct function_traits<Ret(T::*)(Args...) ext> : function_traits<Ret(T::*)(Args...)>{};
	VariantMemberFunc(&);
	VariantMemberFunc(&&);
	VariantMemberFunc(const);
	VariantMemberFunc(const &);
	VariantMemberFunc(noexcept);
	VariantMemberFunc(& noexcept);
	VariantMemberFunc(&& noexcept);
	VariantMemberFunc(const noexcept);
	VariantMemberFunc(const& noexcept);

	export
	template <typename Ty>
		requires std::is_class_v<Ty>
	struct function_traits<Ty> : function_traits<std::remove_pointer_t<decltype(&Ty::operator())>>{

	};

	export
	template <std::size_t N, typename FuncType>
	struct function_arg_at{
		using trait = function_traits<FuncType>;
		static_assert(N < trait::args_count, "error: invalid parameter index.");
		using type = std::tuple_element_t<N, typename trait::args_type>;
	};

	export
		template <typename FuncType>
		struct function_arg_at_last{
		using trait = function_traits<FuncType>;
		using type = std::tuple_element_t<trait::args_count - 1, typename trait::args_type>;
	};

	template <std::size_t I, std::size_t size, typename ArgTuple, typename DefTuple>
	constexpr decltype(auto) getWithDef(ArgTuple&& argTuple, DefTuple&& defTuple){
		if constexpr(I < size){
			return std::get<I>(std::forward<ArgTuple>(argTuple));
		} else{
			return std::get<I>(std::forward<DefTuple>(defTuple));
		}
	}

	template <typename TargetTuple, typename... Args, std::size_t... I>
	constexpr decltype(auto) makeTuple_withDef_impl(std::tuple<Args...>&& args, TargetTuple&& defaults,
	                                                std::index_sequence<I...>){
		return std::make_tuple(std::tuple_element_t<I, std::decay_t<TargetTuple>>{
				ext::getWithDef<I, sizeof...(Args)>(std::move(args), std::forward<TargetTuple>(defaults))
			}...);
	}

	export
	template <typename Src, typename Dst>
		requires requires(Src&& src){
			Dst{std::forward<Src>(src)};
		}
	struct Projection{
		constexpr Dst operator()(Src&& src) const noexcept(noexcept(Dst{std::forward<Src>(src)})){
			return Dst{std::forward<Src>(src)};
		}
	};

	export
	template <typename Dst>
	struct strict_to{
		template <typename T>
		constexpr Dst operator()(T&& src) const noexcept(noexcept(Dst{std::forward<T>(src)})){
			static_assert(std::convertible_to<Dst, std::decay_t<T>>);
			return Dst{std::forward<T>(src)};
		}
	};

	export
	template <typename Dst>
	struct to{
		template <typename T>
		constexpr Dst operator()(T&& src) const noexcept(noexcept(Dst{std::forward<T>(src)})){
			return Dst{std::forward<T>(src)};
		}
	};

	template <typename T, T a, T b>
	constexpr T max_const = a > b ? a : b;

	template <typename T, T a, T b>
	constexpr T min_const = a < b ? a : b;

	template <typename TargetTuple, typename FromTuple, std::size_t... I>
	constexpr bool tupleConvertableTo(std::index_sequence<I...>){
		return (std::convertible_to<std::tuple_element_t<I, FromTuple>, std::tuple_element_t<I, TargetTuple>> && ...);
	}

	template <typename TargetTuple, typename FromTuple, std::size_t... I>
	constexpr bool tupleSameAs(std::index_sequence<I...>){
		return (std::same_as<std::tuple_element_t<I, FromTuple>, std::tuple_element_t<I, TargetTuple>> && ...);
	}


	template <typename TargetTuple, typename T, std::size_t... I>
	constexpr bool tupleContains(std::index_sequence<I...>){
		return (std::same_as<T, std::tuple_element_t<I, TargetTuple>> || ...);
	}

	template <typename T, typename... Ts>
	struct UniqueTypeIndex;

	template <typename T, typename... Ts>
	struct UniqueTypeIndex<T, T, Ts...> : std::integral_constant<std::size_t, 0>{};

	template <typename T, typename U, typename... Ts>
	struct UniqueTypeIndex<T, U, Ts...> : std::integral_constant<std::size_t, 1 + UniqueTypeIndex<T, Ts...>::value>{};

	export
	template <typename T, typename... Ts>
	constexpr std::size_t uniqueTypeIndex_v = UniqueTypeIndex<T, Ts...>::value;


	template <bool Test, auto val1, decltype(val1) val2>
	struct conditional_constexpr_val{
		static constexpr auto value = val1;
	};

	template <auto val1, decltype(val1) val2>
	struct conditional_constexpr_val<false, val1, val2>{
		static constexpr auto value = val1;
	};

	export
	template <bool Test, auto val1, decltype(val1) val2>
	constexpr auto conditional_v = conditional_constexpr_val<Test, val1, val2>::value;

	export
	template <typename T, typename = void>
	struct is_complete_type : std::false_type{};

	template <typename T>
	struct is_complete_type<T, decltype(void(sizeof(T)))> : std::true_type{};

	export
	template <typename T>
	constexpr bool is_complete = is_complete_type<T>::value;
}


export namespace ext{
	template <typename SuperTuple, typename... Args>
	constexpr decltype(auto) make_tuple_with_def(SuperTuple&& defaultArgs, Args&&... args){
		return ext::makeTuple_withDef_impl(std::make_tuple(std::forward<Args>(args)...),
		                                   std::forward<SuperTuple>(defaultArgs),
		                                   std::make_index_sequence<std::tuple_size_v<std::decay_t<SuperTuple>>>());
	}

	template <typename SuperTuple, typename... Args>
	constexpr decltype(auto) make_tuple_with_def(Args&&... args){
		return ext::make_tuple_with_def(SuperTuple{}, std::forward<Args>(args)...);
	}

	/**
	 * @brief
	 * @tparam strict Using std::same_as or std::convertable_to
	 * @tparam SuperTuple Super sequence of the params
	 * @tparam Args given params
	 * @return Whether given param types are the subseq of the SuperTuple
	 */
	template <bool strict, typename SuperTuple, typename... Args>
	constexpr bool is_types_sub_of(){
		constexpr std::size_t fromSize = sizeof...(Args);
		constexpr std::size_t toSize = std::tuple_size_v<SuperTuple>;
		if constexpr(std::tuple_size_v<SuperTuple> < fromSize) return false;

		if constexpr(strict){
			return ext::tupleSameAs<SuperTuple, std::tuple<std::decay_t<Args>...>>(
				std::make_index_sequence<ext::min_const<std::size_t, toSize, fromSize>>());
		} else{
			return ext::tupleConvertableTo<SuperTuple, std::tuple<std::decay_t<Args>...>>(
				std::make_index_sequence<ext::min_const<std::size_t, toSize, fromSize>>());
		}
	}

	template <typename T, typename ArgsTuple>
	constexpr bool contained_in = requires{
		requires ext::tupleContains<ArgsTuple, T>(std::make_index_sequence<std::tuple_size_v<ArgsTuple>>());
	};

	template <typename T, typename... Args>
	constexpr bool contained_within = requires{
		requires ext::tupleContains<std::tuple<Args...>, T>(std::make_index_sequence<std::tuple_size_v<std::tuple<Args...>>>());
	};

	template <typename SuperTuple, typename FromTuple>
	constexpr bool is_tuple_sub_of(){
		constexpr std::size_t fromSize = std::tuple_size_v<FromTuple>;
		constexpr std::size_t toSize = std::tuple_size_v<SuperTuple>;
		if constexpr(toSize < fromSize) return false;

		return ext::tupleConvertableTo<SuperTuple, FromTuple>(
			std::make_index_sequence<ext::min_const<std::size_t, toSize, fromSize>>());
	}

	template <typename MemberPtr>
	struct mptr_info;

	template <typename C, typename T>
	struct mptr_info<T C::*>{
		using class_type = C;
		using value_type = T;
	};

	template <typename C, typename T, typename... Args>
	struct mptr_info<T (C::*)(Args...)>{
		using class_type = C;
		using value_type = T;
		using func_args = std::tuple<Args...>;
	};

	template <typename C, typename T>
	struct mptr_info<T C::* const> : mptr_info<T C::*>{};

	template <typename C, typename T>
	struct mptr_info<T C::* &> : mptr_info<T C::*>{};

	template <typename C, typename T>
	struct mptr_info<T C::* &&> : mptr_info<T C::*>{};

	template <typename C, typename T>
	struct mptr_info<T C::* const&> : mptr_info<T C::*>{};



	template <typename T>
	concept default_hashable = requires(const T& t){
		requires std::is_default_constructible_v<std::hash<T>>;
		requires requires(const std::hash<T>& hasher){
			{ hasher.operator()(t) } noexcept -> std::same_as<std::size_t>;
		};
	};


	template <bool Test, class T>
	struct ConstConditional{
		using type = T;
	};


	template <class T>
	struct ConstConditional<true, T>{
		using type = std::add_const_t<T>;
	};


	template <class T>
	struct ConstConditional<true, T&>{
		using type = std::add_lvalue_reference_t<std::add_const_t<T>>;
	};


	template <typename T>
	constexpr bool isConstRef = std::is_const_v<std::remove_reference_t<T>>;

	template <typename T>
	struct is_const_lvalue_reference{
		static constexpr bool value = false;
	};

	template <typename T>
	struct is_const_lvalue_reference<T&>{
		static constexpr bool value = false;
	};

	template <typename T>
	struct is_const_lvalue_reference<const T&>{
		static constexpr bool value = true;
	};

	template <typename T>
	struct is_const_lvalue_reference<const volatile T&>{
		static constexpr bool value = true;
	};

	template <typename T>
	constexpr bool is_const_lvalue_reference_v = is_const_lvalue_reference<T>::value;

	//
	// template <auto... ptr>
	// struct SeqMemberPtrAccessor;
	//
	//
	// template <auto headPtr, auto... ptr>
	// struct SeqMemberPtrAccessor<headPtr, ptr...>{
	// 	static constexpr auto size = sizeof...(ptr) + 1;
	// 	static_assert(size > 0);
	// 	using TupleType = std::tuple<decltype(headPtr), decltype(ptr)...>;
	// 	static constexpr std::tuple ptrs = std::make_tuple(headPtr, ptr...);
	//
	// 	using RetType = typename GetMemberPtrType<std::tuple_element_t<size - 1, TupleType>>::type;
	// 	using InType = typename GetMemberPtrInfo<std::tuple_element_t<0, TupleType>>::type;
	// 	using NextInType = typename GetMemberPtrType<std::tuple_element_t<0, TupleType>>::type;
	//
	// 	static constexpr bool isPtr = std::is_pointer_v<InType>;
	// 	using NextGroupType = SeqMemberPtrAccessor<ptr...>;
	//
	// 	template <typename T>
	// 		requires std::same_as<std::remove_cvref_t<T>, InType>
	// 	static auto getNext(T& in) -> typename ConstConditional<isConstRef<T>, NextInType&>::type{
	// 		if constexpr(isPtr){
	// 			if(in != nullptr){
	// 				return in->*headPtr;
	// 			}
	// 			throw NullPointerException{};
	// 		}
	// 		return in.*headPtr;
	// 	}
	//
	// 	template <typename T>
	// 		requires std::same_as<std::remove_cvref_t<T>, InType>
	// 	static decltype(auto) get(T& in){
	// 		return NextGroupType::get(SeqMemberPtrAccessor::getNext(in));
	// 	}
	// };
	//
	//
	// template <auto headPtr>
	// struct SeqMemberPtrAccessor<headPtr>{
	// 	using RetType = typename GetMemberPtrType<decltype(headPtr)>::type;
	// 	using InType = typename GetMemberPtrInfo<decltype(headPtr)>::type;
	// 	static constexpr bool isPtr = std::is_pointer_v<InType>;
	//
	// 	template <typename T>
	// 		requires std::same_as<std::remove_cvref_t<T>, InType>
	// 	static auto getNext(T& in) -> typename ConstConditional<isConstRef<T>, RetType&>::type{
	// 		if constexpr(isPtr){
	// 			if(in != nullptr){
	// 				return in->*headPtr;
	// 			}
	// 			throw NullPointerException{};
	// 		}
	// 		return in.*headPtr;
	// 	}
	//
	// 	template <typename T>
	// 		requires std::same_as<std::remove_cvref_t<T>, InType>
	// 	static decltype(auto) get(T& in){
	// 		return SeqMemberPtrAccessor::getNext(in);
	// 	}
	// };
}

//flatten_tuple
namespace ext{
	export
	template <typename T>
	struct is_tuple : std::false_type{};

	template <typename... Ts>
	struct is_tuple<std::tuple<Ts...>> : std::true_type{};

	export
	template <typename T>
	constexpr bool is_tuple_v = is_tuple<T>::value;

	template <std::size_t idx, typename Ts>
	struct flatten_tuple_impl{
		using type = std::tuple<Ts>;
		static constexpr std::size_t index = idx;
		static constexpr std::size_t stride = 1;

		using args = flatten_tuple_impl;

		template <std::size_t Idx, bool byRef = true, typename T>
		static constexpr decltype(auto) at(T&& t) noexcept{
			return std::forward<T>(t);
		}

		template <typename T>
		static constexpr T& forward_by_ref(T& t){
			return t;
		}

		template <typename T>
		static constexpr const T& forward_by_ref(const T& t){
			return t;
		}

		template <typename T>
		static constexpr T& forward_by_ref(T&& t) = delete;
	};

	template <std::size_t curIndex, typename... Ts>
	struct flatten_tuple_impl<curIndex, std::tuple<Ts...>>{
		using type = decltype(std::tuple_cat(std::declval<typename flatten_tuple_impl<0, Ts>::type>()...));
		static constexpr std::size_t index = curIndex;
		static constexpr std::size_t stride = (flatten_tuple_impl<0, Ts>::stride + ...);

	private:
		template <std::size_t ... I>
		static constexpr std::size_t stride_at(std::index_sequence<I...>){
			return (flatten_tuple_impl<0, std::tuple_element_t<I, std::tuple<Ts...>>>::stride + ... + 0);
		}

		static consteval auto unwrap_one(){
			return [&] <std::size_t ... I>(std::index_sequence<I...>){
				return
					std::tuple_cat(
						std::make_tuple(
							flatten_tuple_impl<flatten_tuple_impl::stride_at(std::make_index_sequence<I>()), std::tuple_element_t<
								              I, std::tuple<Ts...>>>{}...),
						std::make_tuple(flatten_tuple_impl<stride, void>{}));
			}(std::index_sequence_for<Ts...>());
		}
	public:
		using args = decltype(unwrap_one());

		template <std::size_t Idx, bool byRef = true, typename T>
			requires std::same_as<std::decay_t<T>, std::tuple<Ts...>>
		static constexpr decltype(auto) at(T&& t) noexcept{
			constexpr std::size_t validIndex = []<std::size_t ... I>(std::index_sequence<I...>){
				std::size_t rst{std::numeric_limits<std::size_t>::max()};
				((func<Idx, I>(rst)), ...);
				return rst;
			}(std::make_index_sequence<stride>());

			static_assert(validIndex != std::numeric_limits<std::size_t>::max(), "invalid index");

			using arg = std::tuple_element_t<validIndex, args>;
			return arg::template at<Idx - arg::index, byRef>(std::get<validIndex>(std::forward<T>(t)));
		}

	private:
		template <std::size_t searchI, std::size_t intervalI>
		static constexpr bool is_index_valid = requires{
			requires searchI >= std::tuple_element_t<intervalI, args>::index;
			requires searchI < std::tuple_element_t<intervalI + 1, args>::index;
		};

		template <std::size_t searchI, std::size_t intervalI>
		static constexpr void func(std::size_t& rst){
			if constexpr(is_index_valid<searchI, intervalI>){
				rst = intervalI;
			}
		}
	};

	export
	template <typename... Ts>
	using flatten_tuple = flatten_tuple_impl<0, Ts...>;

	template <typename T>
	struct to_ref_tuple{
		static consteval auto impl(T& t){
			return [&] <std::size_t ... I>(std::index_sequence<I...>){
				return std::make_tuple(std::ref(std::get<I>(t))...);
			}(std::make_index_sequence<std::tuple_size_v<T>>());
		}

		using type = decltype(to_ref_tuple::impl(std::declval<T&>()));
	};

	export
	template <typename T>
	using to_ref_tuple_t = typename to_ref_tuple<T>::type;

	export
	template <typename... T>
	using flatten_tuple_t = typename flatten_tuple<std::tuple<T...>>::type;

	export
	template <bool toRef, typename T>
	constexpr std::conditional_t<toRef, to_ref_tuple_t<flatten_tuple_t<std::decay_t<T>>>, flatten_tuple_t<std::decay_t<T>>>
	flat_tuple(T&& t) noexcept {
		using flatter = flatten_tuple<std::decay_t<T>>;
		if constexpr (toRef){
			return [&] <std::size_t ...I>(std::index_sequence<I...>){
				return std::make_tuple(std::ref(flatter::template at<I, toRef>(std::forward<T>(t))) ...);
			}(std::make_index_sequence<flatter::stride>());
		}else{
			return [&] <std::size_t ...I>(std::index_sequence<I...>){
				return flatten_tuple_t<std::decay_t<T>>(flatter::template at<I, toRef>(std::forward<T>(t)) ...);
			}(std::make_index_sequence<flatter::stride>());
		}
	}

	export
	template <bool toRef = false>
	struct tuple_flatter{
		template <typename T>
		constexpr decltype(auto) operator()(T&& t) const noexcept{
			return ext::flat_tuple<toRef>(std::forward<T>(t));
		}
	};

	void foo(){
		using Tuple = const std::tuple<std::tuple<long long, std::string&>, std::ranges::subrange<std::vector<std::string>::const_iterator>>;

		using T = decltype(flat_tuple<false>(std::declval<Tuple>()));

		// auto v = flatten_tuple<0, Tuple>::at<1>(tuple);

		// auto f = flatten<true>(tuple);
	}
}

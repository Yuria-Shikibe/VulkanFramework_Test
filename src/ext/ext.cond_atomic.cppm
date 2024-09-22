//
// Created by Matrix on 2024/9/22.
//

export module ext.cond_atomic;

import std;

export namespace ext{
	template<typename T>
	struct is_atomic : std::false_type {};

	template<typename T>
	struct is_atomic<std::atomic<T>> : std::true_type {};

	template<typename T>
	constexpr bool is_atomic_v = is_atomic<T>::value;

	template<typename T>
	struct atomic_identity : std::type_identity<T>{};

	template<typename T>
	struct atomic_identity<std::atomic<T>> : std::type_identity<T>{};

	template<typename T>
	using atomic_identity_t = typename atomic_identity<T>::type;

	template <typename T, bool cond>
		// requires requires{
		// 	requires !cond || requires{
		// 		requires std::is_atomic
		// 		typename std::atomic<T>;
		// 		std::atomic<T>{};
		// 	};
		// }
	using cond_atomic = std::conditional_t<cond, std::atomic<T>, T>;

	using T = atomic_identity_t<int>;

	template <typename T>
	constexpr atomic_identity_t<T> adapted_atomic_exchange(T& src, atomic_identity_t<T>&& val) noexcept{
		if constexpr (is_atomic_v<T>){
			if constexpr (std::is_trivial_v<T>){
				return src.exchange(val);
			}else{
				return src.exchange(std::move(val));
			}
		}else{
			return std::exchange(src, std::move(val));
		}
	}
}
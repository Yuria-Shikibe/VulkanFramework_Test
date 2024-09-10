export module ext.owner;

import std;

export namespace ext{
	/**
	 * @brief mark this pointer has the ownership of the memory
	 * @tparam T * target pointer type
	 */
	template<typename T>
		requires (std::is_pointer_v<T> && !std::is_unbounded_array_v<T>)
	using owner = T;

	/**
	 * @brief mark this pointer has the ownership of the memory, in array
	 * @tparam T [] target pointer type
	 */
	template<typename T>
		requires (std::is_unbounded_array_v<T>)
	using owner_array = T;
}

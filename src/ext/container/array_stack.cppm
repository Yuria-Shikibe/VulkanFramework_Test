export module ext.array_stack;

import std;

export namespace ext{
	//OPTM replace with inplace array

	/**
	 * @brief stack in fixed size, provide bound check ONLY in debug mode
	 * @warning do not destruct element after pop
	 * @tparam T type
	 * @tparam N capacity
	 */
	template<typename T, std::size_t N>
		requires (std::is_default_constructible_v<T> && N > 0)
	struct array_stack{
	private:
		std::array<T, N> items{};
		std::size_t sz{};

	public:
		[[nodiscard]] constexpr std::size_t size() const noexcept{
			return sz;
		}

		[[nodiscard]] constexpr bool empty() const noexcept{
			return sz == 0;
		}

		[[nodiscard]] constexpr bool full() const noexcept{
			return sz == N;
		}

		[[nodiscard]] constexpr std::size_t capacity() const noexcept{
			return items.size();
		}

		constexpr void push(const T& val) noexcept(noexcept(checkUnderFlow()) && std::is_nothrow_copy_assignable_v<T>) {
			checkOverFlow();
			items[sz++] = val;
		}

		constexpr void push(T&& val) noexcept(noexcept(checkUnderFlow()) && std::is_nothrow_move_assignable_v<T>) {
			checkOverFlow();
			items[sz++] = std::move(val);
		}

		[[nodiscard]] constexpr decltype(auto) top() noexcept(noexcept(checkUnderFlow())){
			checkUnderFlow();

			return items[sz - 1];
		}

		[[nodiscard]] constexpr decltype(auto) top() const noexcept(noexcept(checkUnderFlow())){
			checkUnderFlow();

			return items[sz - 1];
		}

		constexpr void pop() noexcept(noexcept(checkUnderFlow())){
			checkUnderFlow();

			--sz;
		}

		[[nodiscard]] constexpr std::optional<T> try_pop_and_get() noexcept(std::is_move_constructible_v<T>){
			if(empty())return std::nullopt;

			std::optional<T> t{std::move(items[--sz])};
			return t;
		}

		void clear() noexcept{
			std::ranges::destroy_n(items.begin(), sz);
			std::ranges::uninitialized_default_construct_n(items.begin(), sz);
			sz = 0;
		}

	private:
		void checkUnderFlow() const noexcept(!DEBUG_CHECK){
#if DEBUG_CHECK
			if(sz == 0){
				throw std::underflow_error("array stack is empty");
			}
#endif
		}

		constexpr void checkOverFlow() const noexcept(!DEBUG_CHECK){
#if DEBUG_CHECK
			if(sz >= N){
				throw std::overflow_error("array stack is overflow");
			}
#endif
		}
	};
}
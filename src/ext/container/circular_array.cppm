export module ext.circular_array;

import std;

export namespace ext{
	//TODO as container adaptor?
	template <typename T, std::size_t size>
		requires (std::is_default_constructible_v<T>)
	struct circular_array : std::array<T, size>{
		using std::array<T, size>::array;

	private:
		std::size_t current{};

	public:
		constexpr auto& operator++(){
			next();
			return get();
		}

		constexpr auto& operator++(int){
			auto& cur = get();
			next();
			return cur;
		}

		constexpr auto& operator--(){
			prev();
			return get();
		}

		constexpr auto& operator--(int){
			auto& cur = get();
			prev();
			return cur;
		}

		constexpr void next(){
			current = (current + 1) % size;
		}

		constexpr void prev(){
			current = (current - 1 + size) % size;
		}

		constexpr void next(typename std::array<T, size>::difference_type off){
			current = (current + off % size) % size;
		}

		constexpr void prev(typename std::array<T, size>::difference_type off){
			current = (current - off % size + size) % size;
		}

		[[nodiscard]] constexpr decltype(auto) get() noexcept{
			return this->array::operator[](current);
		}

		[[nodiscard]] constexpr std::size_t get_index() const noexcept{
			return current;
		}

		[[nodiscard]] constexpr decltype(auto) get() const noexcept{
			return this->array::operator[](current);
		}

		[[nodiscard]] constexpr decltype(auto) operator[](const std::size_t index) const noexcept{
			return this->array::operator[](index % size);
		}

		[[nodiscard]] constexpr decltype(auto) operator[](const std::size_t index) noexcept{
			return this->array::operator[](index % size);
		}
	};
}

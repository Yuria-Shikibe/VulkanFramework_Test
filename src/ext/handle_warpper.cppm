export module ext.handle_wrapper;
import std;

export namespace ext{
	template <typename T, bool enableCopy = false>
		requires std::is_pointer_v<T>
	struct wrapper{
	protected:
		T handle{};

	public:
		using type = T;

		[[nodiscard]] constexpr wrapper() = default;

		[[nodiscard]] constexpr explicit(false) wrapper(const T handler)
			: handle{handler}{}

		constexpr T release(){
			return std::exchange(handle, nullptr);
		}

		constexpr wrapper(const wrapper& other) requires (enableCopy) = default;

		constexpr wrapper& operator=(const wrapper& other) requires (enableCopy) = default;

		constexpr wrapper(wrapper&& other) noexcept
			: handle{std::move(other.handle)}{
			other.handle = nullptr;
		}

		constexpr wrapper& operator=(wrapper&& other) noexcept{
			if(this == &other) return *this;
			std::swap(handle, other.handle);
			return *this;
		}

		constexpr wrapper& operator=(T other) noexcept{
			if(this->handle == other) return *this;
			return this->operator=(wrapper<T>{other});
		}

		[[nodiscard]] constexpr explicit(false) operator T() const noexcept{ return handle; }

		[[nodiscard]] constexpr explicit(false) operator bool() const noexcept{ return handle != nullptr; }

		[[nodiscard]] constexpr T operator->() const noexcept{ return handle; }

		[[nodiscard]] constexpr T get() const noexcept{ return handle; }

		[[nodiscard]] constexpr std::array<T, 1> as_seq() const noexcept{ return std::array{handle}; }

		[[nodiscard]] constexpr const T* as_data() const noexcept{ return &handle; }

		[[nodiscard]] constexpr T* as_data() noexcept{ return &handle; }
	};
	/**
	 * @brief Make move constructor default declarable
	 * @tparam T Dependency Type
	 */
	template <typename T>
		requires std::is_pointer_v<T>
	struct dependency{
		using type = T;

		T handle{};

		[[nodiscard]] constexpr dependency() noexcept = default;

		[[nodiscard]] constexpr explicit(false) dependency(T device) noexcept : handle{device}{}

		constexpr ~dependency() = default;

		[[nodiscard]] constexpr explicit(false) operator T() const noexcept{ return handle; }

		[[nodiscard]] constexpr explicit(false) operator bool() const noexcept{ return handle != nullptr; }

		[[nodiscard]] constexpr T operator->() const noexcept{ return handle; }

		[[nodiscard]] constexpr const T* asData() const noexcept{ return &handle; }

		constexpr dependency(const dependency& other) = delete;

		constexpr dependency(dependency&& other) noexcept
			: handle{other.handle}{
			other.handle = nullptr;
		}

		constexpr dependency& operator=(const dependency& other) = delete;

		constexpr dependency& operator=(dependency&& other) noexcept{
			if(this == &other) return *this;
			std::swap(handle, other.handle);
			return *this;
		}

		constexpr dependency& operator=(T other) noexcept{
			if(this->handle == other) return *this;
			return this->operator=(dependency{other});
		}
	};
}

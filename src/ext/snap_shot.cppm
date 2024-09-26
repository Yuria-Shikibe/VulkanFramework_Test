//
// Created by Matrix on 2024/6/14.
//

export module ext.snap_shot;

import std;

export namespace ext{
	/**
	 * @brief provide a type for state resume or apply
	 *
	 *	cap - captured value
	 *	snap - snapshot value
	 *
	 * @tparam T value type
	 */
	template <typename T>
		requires (std::is_copy_assignable_v<T>)
	struct snap_shot{
		static constexpr bool is_nothrow_copy_assignable = std::is_nothrow_copy_assignable_v<T>;
		static constexpr bool is_nothrow_equality_comparable = requires(const T& t){
			requires std::equality_comparable<T>;
			{ t == t } noexcept;
		};

		using value_type = T;

		T base{};
		T temp{}; //TODO should it be mutable?

		[[nodiscard]] constexpr snap_shot() requires (std::is_default_constructible_v<T>) = default;

		[[nodiscard]] constexpr explicit snap_shot(const T& ref) : base{ref}, temp{ref}{}

		constexpr void resume() noexcept(is_nothrow_copy_assignable) {
			temp = base;
		}

		constexpr bool try_resume() noexcept(is_nothrow_copy_assignable && is_nothrow_equality_comparable) requires (std::equality_comparable<T>) {
			if(temp == base)return false;
			resume();
			return true;
		}

		constexpr bool try_apply() noexcept(is_nothrow_copy_assignable && is_nothrow_equality_comparable) requires (std::equality_comparable<T>) {
			if(temp == base)return false;
			apply();
			return true;
		}

		template <typename R>
		constexpr void resumeProj(R T::* mptr) requires requires{
			requires std::is_copy_assignable_v<std::invoke_result_t<decltype(mptr), T&>>;
		}{
			std::invoke_r<R&>(mptr, temp) = std::invoke_r<const R&>(mptr, base);
		}

		constexpr void apply() noexcept(is_nothrow_copy_assignable) {
			base = temp;
		}

		constexpr void apply() && noexcept(std::is_nothrow_move_assignable_v<T>) requires (std::is_move_assignable_v<T>) {
			base = std::move(temp);
		}

		template <typename R>
		constexpr void apply_proj(R T::* mptr) requires requires{
			requires std::is_copy_assignable_v<std::invoke_result_t<decltype(mptr), T&>>;
		}{
			std::invoke_r<R&>(mptr, base) = std::invoke_r<const R&>(mptr, temp);
		}

		constexpr void set(const T& val) noexcept(is_nothrow_copy_assignable) {
			base = temp = val;
		}

		constexpr snap_shot& operator=(const T& val) noexcept(is_nothrow_copy_assignable) {
			snap_shot::set(val);
			return *this;
		}

		constexpr T* operator ->() noexcept{
			return &base;
		}

		constexpr const T* operator ->() const noexcept{
			return &base;
		}

		constexpr snap_shot(const snap_shot& other) noexcept(is_nothrow_copy_assignable) = default;

		constexpr snap_shot(snap_shot&& other) noexcept = default;

		constexpr snap_shot& operator=(const snap_shot& other) noexcept(is_nothrow_copy_assignable) = default;

		constexpr snap_shot& operator=(snap_shot&& other) noexcept = default;

		constexpr friend bool operator==(const snap_shot& lhs, const snap_shot& rhs)
			noexcept(is_nothrow_equality_comparable)
			requires (std::equality_comparable<T>){
			return lhs.base == rhs.base;
		}
	};
}


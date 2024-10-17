export module ext.atomic_caller;

import std;

export namespace ext{
	template <typename T, typename AcquireType = const T&>
		requires std::convertible_to<T, AcquireType>
	struct atomic_caller{
		using value_type = T;
		using return_type = AcquireType;

	private:
		std::atomic_flag value_lk{};
		value_type expected{};

		void reset() noexcept{
			value_lk.clear(std::memory_order::release);
			value_lk.notify_all();
		}

	public:
		[[nodiscard]] std::optional<value_type> test() const noexcept(std::is_nothrow_copy_assignable_v<T>) requires (std::is_copy_assignable_v<T>){
			if(!value_lk.test(std::memory_order::acquire)){
				return expected;
			}

			return std::nullopt;
		}

		[[nodiscard]] return_type wait_and_get() const noexcept{
			value_lk.wait(true, std::memory_order::acquire);

			return expected;
		}

		template <std::invocable<> Func, std::predicate<const value_type&> Pred>
			requires std::assignable_from<value_type&, std::invoke_result_t<Func>>
		return_type exec_if_not(Func func, Pred cancel)
			noexcept(
				std::is_nothrow_invocable_v<Func> &&
				std::is_nothrow_invocable_v<Pred, const value_type&> &&
				std::is_nothrow_assignable_v<value_type&, std::invoke_result_t<Func>>
			){
			if(!value_lk.test_and_set(std::memory_order::acq_rel)){
				if(!std::invoke(cancel, expected)){
					expected = std::invoke(func);
				}

				reset();
				return expected;
			}

			return wait_and_get();
		}

		template <std::invocable<> Func>
			requires std::assignable_from<value_type&, std::invoke_result_t<Func>>
		return_type exec(Func func) noexcept(
			std::is_nothrow_invocable_v<Func> &&
			std::is_nothrow_assignable_v<value_type&, std::invoke_result_t<Func>>
		){
			if(!value_lk.test_and_set(std::memory_order::acq_rel)){
				expected = std::invoke(func);

				reset();
				return expected;
			}

			return wait_and_get();
		}

		template <std::invocable<> Func, std::predicate<value_type> Pred>
			requires std::assignable_from<value_type&, std::invoke_result_t<Func>>
		return_type exec_if(Func func, Pred pred)
			noexcept(
				std::is_nothrow_invocable_v<Func> &&
				std::is_nothrow_invocable_v<Pred, const value_type&> &&
				std::is_nothrow_assignable_v<value_type&, std::invoke_result_t<Func>>){
			return this->exec_if_not(func, std::not_fn(pred));
		}

		[[nodiscard]] atomic_caller() = default;

		atomic_caller(const atomic_caller& other) noexcept{}

		atomic_caller(atomic_caller&& other) noexcept{}

		atomic_caller& operator=(const atomic_caller& other) noexcept{
			return *this;
		}

		atomic_caller& operator=(atomic_caller&& other) noexcept{
			return *this;
		}
	};
}

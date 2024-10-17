module;

export module ext.fast_pop_stack;

import ext.array_stack;

import std;

export namespace ext{

	template <typename T, std::size_t each_size, std::size_t group_size>
	requires (std::is_default_constructible_v<T>)
struct fast_pop_stack{
	struct access_unit{
		std::mutex mutex{};
		array_stack<T, each_size> stack{};
	};

	std::array<access_unit, group_size> data{};
	std::atomic_size_t counter{};

	/**
	 * @brief
	 * @return valid optional value if push failed
	 */
	template <typename... Args>
		requires (std::constructible_from<T, Args...> && std::is_move_assignable_v<T>)
	std::optional<T> push(
		Args&&... args) noexcept(noexcept(T{std::forward<Args>(args)...}) && std::is_nothrow_move_assignable_v<T>){
		return fast_pop_stack::push(T{std::forward<Args>(args)...});
	}


	/**
	 * @brief
	 * @return valid optional value if push failed
	 */
	template <typename Ty>
		requires (std::same_as<std::decay_t<Ty>, T> && std::constructible_from<T, Ty> && std::is_move_assignable_v<T>)
	std::optional<T> push(Ty&& input_data) noexcept(std::is_nothrow_move_assignable_v<T>){
		const auto t = counter++;

		if (t & 1){
			for(access_unit& unit : data){
				if(unit.mutex.try_lock()){
					std::lock_guard guard{unit.mutex, std::adopt_lock};

					if(unit.stack.full()){
						continue;
					}

					unit.stack.push(std::forward<Ty>(input_data));
					return std::nullopt;
				}
			}
		}else{
			for(access_unit& unit : data | std::views::reverse){
				if(unit.mutex.try_lock()){
					std::lock_guard guard{unit.mutex, std::adopt_lock};

					if(unit.stack.full()){
						continue;
					}

					unit.stack.push(std::forward<Ty>(input_data));
					return std::nullopt;
				}
			}
		}

		access_unit& unit = data[t % group_size];
		std::lock_guard guard{unit.mutex};

		if(unit.stack.full()){
			return std::forward<Ty>(input_data);
		}

		unit.stack.push(std::forward<Ty>(input_data));
		return std::nullopt;
	}

	std::optional<T> pop() noexcept(std::is_nothrow_move_assignable_v<T>){
		const auto t = counter++;

		if (t & 1){
			for(access_unit& unit : data){
				if(unit.mutex.try_lock()){
					std::lock_guard guard{unit.mutex, std::adopt_lock};

					if(auto val = unit.stack.try_pop_and_get()){
						return val;
					}
				}
			}
		}else{
			for(access_unit& unit : data | std::views::reverse){
				if(unit.mutex.try_lock()){
					std::lock_guard guard{unit.mutex, std::adopt_lock};

					if(auto val = unit.stack.try_pop_and_get()){
						return val;
					}
				}
			}
		}

		access_unit& unit = data[t % group_size];
		std::lock_guard guard{unit.mutex};
		return unit.stack.try_pop_and_get();
	}

	void clear() noexcept{
		for(access_unit& unit : data){
			std::lock_guard guard{unit.mutex};

			unit.stack.clear();
		}
	}
};
}

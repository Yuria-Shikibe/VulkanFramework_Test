export module ext.Guard;

import ext.meta_programming;
import ext.Concepts;
import std;

export namespace ext{
	template <typename T, auto T::* mem, bool passByMove = false>
		requires requires{
			requires
				(passByMove && std::is_move_assignable_v<typename mptr_info<decltype(mem)>::value_type>) ||
				(!passByMove && std::is_copy_assignable_v<typename mptr_info<decltype(mem)>::value_type>);
		}
	class [[jetbrains::guard]] Guard{
		using DataType = typename mptr_info<decltype(mem)>::value_type;

		T& tgt;
		DataType original{};
	public:
		[[nodiscard]] constexpr Guard(T& tgt, const DataType& data) requires (!passByMove) : tgt{tgt}, original{std::invoke(mem, tgt)}{
			std::invoke(mem, tgt) = data;
		}

		[[nodiscard]] constexpr Guard(T& tgt, DataType&& data) requires (passByMove) : tgt{tgt}, original{std::move(std::invoke(mem, tgt))}{
			std::invoke(mem, tgt) = std::move(data);
		}

		constexpr ~Guard(){
			if constexpr (passByMove){
				std::invoke(mem, tgt) = std::move(original);
			}else{
				std::invoke(mem, tgt) = original;
			}
		}
	};

	template <typename T, bool passByMove = false>
		requires requires{
		requires
			(passByMove && std::is_move_assignable_v<T>) ||
			(!passByMove && std::is_copy_assignable_v<T>);
		}
	class [[jetbrains::guard]] GuardRef{
		T& tgt;
		T original;

	public:
		[[nodiscard]] constexpr GuardRef(T& tgt, const T& data) requires (!passByMove) : tgt{tgt}, original{tgt}{
			this->tgt = data;
		}

		[[nodiscard]] constexpr GuardRef(T& tgt, T&& data) requires (passByMove) : tgt{tgt}, original{std::move(tgt)}{
			this->tgt = std::move(data);
		}

		constexpr ~GuardRef(){
			if constexpr (passByMove){
				tgt = std::move(original);
			}else{
				tgt = original;
			}
		}
	};

	template <typename T>
		requires std::is_trivial_v<T>
	class GuardTrivial{
		T& tgt;
		T original;

	public:
		[[nodiscard]] constexpr GuardTrivial(T& tgt, const T& data) : tgt{tgt}, original{tgt}{
			this->tgt = data;
		}

		constexpr ~GuardTrivial(){
			tgt = original;
		}
	};
}

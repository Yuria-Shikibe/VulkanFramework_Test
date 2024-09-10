export module ext.guard;

import ext.meta_programming;
import std;

export namespace ext{

	template <typename T, bool passByMove = false>
		requires requires{
		requires
			(passByMove && std::is_move_assignable_v<T>) ||
			(!passByMove && std::is_copy_assignable_v<T>);
		}
	class [[jetbrains::guard]] guard{
		T& tgt;
		T original;

	public:
		[[nodiscard]] constexpr guard(T& tgt, const T& data) requires (!passByMove) : tgt{tgt}, original{tgt}{
			this->tgt = data;
		}

		[[nodiscard]] constexpr guard(T& tgt, T&& data) requires (passByMove) : tgt{tgt}, original{std::move(tgt)}{
			this->tgt = std::move(data);
		}

		constexpr ~guard(){
			if constexpr (passByMove){
				tgt = std::move(original);
			}else{
				tgt = original;
			}
		}
	};
}

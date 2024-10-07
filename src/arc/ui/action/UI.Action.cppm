module;

#include "../src/ext/no_unique_address.hpp"

export module Core.UI.Action;

import Math.Timed;
import std;
import ext.meta_programming;

namespace Core::UI{
	export
	template <typename T>
	struct Action{
		using TargetType = T;
		Math::Timed scale{};

	public:
		virtual ~Action() = default;

		const Math::Interp::InterpFunc interpFunc{};

		Action() = default;

		Action(const float lifetime, const Math::Interp::InterpFunc& interpFunc)
			: scale(lifetime),
			  interpFunc(interpFunc){}

		explicit Action(const float lifetime)
			: scale(lifetime){}

		[[nodiscard]] float getProgressOf(const float f) const{
			return interpFunc ? interpFunc(f) : f;
		}

		/**
		 * @return The excess time, it > 0 means this action has been done
		 */
		virtual float update(const float delta, T& elem){
			float ret = -1.0f;

			if(scale.time == 0.0f){
				this->begin(elem);
				scale.time = std::numeric_limits<float>::min();
			}

			if(const auto remain = scale.time - scale.lifetime; remain >= 0){
				return remain + delta;
			}

			scale.time += delta;

			if(const auto remain = scale.time - scale.lifetime; remain >= 0){
				ret = remain;

				scale.time = scale.lifetime;
				this->apply(elem, getProgressOf(1.f));
				this->end(elem);
			}else{
				this->apply(elem, getProgressOf(scale.get()));
			}

			return ret;
		}

		virtual void apply(T& elem, float progress){}

		virtual void begin(T& elem){}

		virtual void end(T& elem){}
	};

	export
	template <typename T>
	struct ParallelAction : Action<T>{

	protected:
		void selectMaxLifetime(){
			for(auto& action : actions){
				this->scale.lifetime = std::max(this->scale.lifetimem, action->scale.lifetime);
			}
		}

	public:
		std::vector<std::unique_ptr<Action<T>>> actions{};

		explicit ParallelAction(std::vector<std::unique_ptr<Action<T>>>&& actions)
			: actions(std::move(actions)){
			selectMaxLifetime();
		}

		ParallelAction(const std::initializer_list<std::unique_ptr<Action<T>>> actions)
			: actions(actions){
			selectMaxLifetime();
		}

		float update(const float delta, T& elem) override{
			std::erase_if(actions, [&](std::unique_ptr<Action<T>>& act){
				return act->update(delta, elem) >= 0;
			});

			return Action<T>::update(delta, elem);
		}
	};

	export
	template <typename T>
	struct AlignedParallelAction : Action<T>{

	protected:
		void selectMaxLifetime(){
			for(auto& action : actions){
				this->scale.lifetime = std::max(this->scale.lifetimem, action->scale.lifetime);
			}
		}

	public:
		std::vector<std::unique_ptr<Action<T>>> actions{};

		explicit AlignedParallelAction(const float lifetime, const Math::Interp::InterpFunc& interpFunc, std::vector<std::unique_ptr<Action<T>>>&& actions)
			: Action<T>{lifetime, interpFunc}, actions(std::move(actions)){
			selectMaxLifetime();
		}

		AlignedParallelAction(const float lifetime, const Math::Interp::InterpFunc& interpFunc, const std::initializer_list<std::unique_ptr<Action<T>>> actions)
			: Action<T>{lifetime, interpFunc}, actions(actions){
			selectMaxLifetime();
		}

		void apply(T& elem, float progress) override{
			for (auto && action : actions){
				action->apply(elem, action.getProgressOf(progress));
			}
		}

		void begin(T& elem) override{
			for (auto && action : actions){
				action->begin(elem);
			}
		}

		void end(T& elem) override{
			for (auto && action : actions){
				action->end(elem);
			}
		}
	};
	
	export
	template <typename T>
	struct DelayAction : Action<T>{
		explicit DelayAction(const float lifetime) : Action<T>(lifetime){}

		float update(const float delta, T& elem) override{
			this->scale.time += delta;

			if(const auto remain = this->scale.time - this->scale.lifetime; remain >= 0){
				return remain;
			}

			return -1.f;
		}
	};

	template <typename T>
	struct EmptyApplyFunc{
		void operator()(T&, float){}
	};

	export
	template <typename T, std::invocable<T&, float> FuncApply, std::invocable<T&> FuncBegin = std::identity, std::invocable<T&> FuncEnd = std::identity>
	struct RunnableAction : Action<T>{
		ADAPTED_NO_UNIQUE_ADDRESS std::decay_t<FuncApply> funcApply{};
		ADAPTED_NO_UNIQUE_ADDRESS std::decay_t<FuncBegin> funcBegin{};
		ADAPTED_NO_UNIQUE_ADDRESS std::decay_t<FuncEnd> funcEnd{};

		[[nodiscard]] RunnableAction() = default;

		explicit RunnableAction(FuncApply&& apply, FuncBegin&& begin = {}, FuncEnd&& end = {}) :
			funcApply{std::forward<FuncApply>(apply)},
			funcBegin{std::forward<FuncBegin>(begin)},
			funcEnd{std::forward<FuncEnd>(end)}{}

		explicit RunnableAction(FuncBegin&& begin = {}, FuncEnd&& end = {}) :
			funcBegin{std::forward<FuncBegin>(begin)},
			funcEnd{std::forward<FuncEnd>(end)}{}

		RunnableAction(const RunnableAction& other)
			noexcept(std::is_nothrow_copy_constructible_v<FuncApply> && std::is_nothrow_copy_constructible_v<FuncBegin> && std::is_nothrow_copy_constructible_v<FuncEnd>)
			requires (std::is_copy_constructible_v<FuncApply> && std::is_copy_constructible_v<FuncBegin> && std::is_copy_constructible_v<FuncEnd>) = default;

		RunnableAction(RunnableAction&& other)
			noexcept(std::is_nothrow_move_constructible_v<FuncApply> && std::is_nothrow_move_constructible_v<FuncBegin> && std::is_nothrow_move_constructible_v<FuncEnd>)
			requires (std::is_move_constructible_v<FuncApply> && std::is_move_constructible_v<FuncBegin> && std::is_move_constructible_v<FuncEnd>) = default;

		RunnableAction& operator=(const RunnableAction& other) 
			noexcept(std::is_nothrow_copy_assignable_v<FuncApply> && std::is_nothrow_copy_assignable_v<FuncBegin> && std::is_nothrow_copy_assignable_v<FuncEnd>)
			requires (std::is_copy_assignable_v<FuncApply> && std::is_copy_assignable_v<FuncBegin> && std::is_copy_assignable_v<FuncEnd>) = default;

		RunnableAction& operator=(RunnableAction&& other)
			noexcept(std::is_nothrow_move_assignable_v<FuncApply> && std::is_nothrow_move_assignable_v<FuncBegin> && std::is_nothrow_move_assignable_v<FuncEnd>)
			requires (std::is_move_assignable_v<FuncApply> && std::is_move_assignable_v<FuncBegin> && std::is_move_assignable_v<FuncEnd>) = default;

		void begin(T& elem) override{
			std::invoke(funcBegin, elem);
		}

		void end(T& elem) override{
			std::invoke(funcEnd, elem);
		}

		void apply(T& elem, float v) override{
			std::invoke(funcApply, elem, v);
		}
	};

	template <typename FuncApply, typename FuncBegin = std::identity, typename FuncEnd = std::identity>
	RunnableAction(FuncApply&&, FuncBegin&&, FuncEnd&&) ->
		RunnableAction<std::decay_t<std::tuple_element_t<0, ext::remove_mfptr_this_args<std::decay_t<FuncApply>>>>,
		               FuncApply, FuncBegin, FuncEnd>;

	template <typename FuncBegin = std::identity, typename FuncEnd = std::identity>
	RunnableAction(FuncBegin&&, FuncEnd&&) ->
		RunnableAction<
			std::decay_t<std::tuple_element_t<0, ext::remove_mfptr_this_args<std::decay_t<FuncBegin>>>>,
			EmptyApplyFunc<std::decay_t<std::tuple_element_t<0, ext::remove_mfptr_this_args<std::decay_t<FuncBegin>>>>>,
			FuncBegin,
			FuncEnd>;
	
	template <typename FuncBegin = std::identity, typename FuncEnd = std::identity>
	RunnableAction(FuncBegin&&, FuncEnd&&) ->
		RunnableAction<
			std::decay_t<std::tuple_element_t<0, ext::remove_mfptr_this_args<std::decay_t<FuncEnd>>>>,
			EmptyApplyFunc<std::decay_t<std::tuple_element_t<0, ext::remove_mfptr_this_args<std::decay_t<FuncEnd>>>>>,
			FuncBegin,
			FuncEnd>;

	void foo(){
		// auto f = [](int&, float) static {};

		// using T = ext::function_without_mfptr_caller_t<decltype(f)>;
		// std::unique_ptr<int> va{};
		// RunnableAction runnable{{}, [p = std::move(va)](int& v){
		// 	v += *p;
		// }};
		//
		// RunnableAction r2 = runnable;
		// static_assert(std::same_as<decltype(runnable)::TargetType, int>);
	}
}

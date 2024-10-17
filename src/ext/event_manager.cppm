module;

#include <cassert>
#include "adapted_attributes.hpp"

export module ext.event;

import std;
import ext.concepts;
import ext.type_map;
import ext.heterogeneous;


//TODO delayed event submitter
namespace ext{
	export struct event_type{};

	export
	template <typename T>
	constexpr std::type_index index_of(){
		return std::type_index(typeid(T));
	}

	export
	template <typename T>
	concept event_argument = /*std::derived_from<T, event_type> &&*/ std::is_final_v<T> || std::is_fundamental_v<T>;

	struct EventProjection{
		template <std::ranges::range Rng>
		static decltype(auto) projection(const Rng& range){
			return range;
		}
	};

	struct event_registry{
		[[nodiscard]] event_registry() noexcept(!DEBUG_CHECK) = default;

#if DEBUG_CHECK
		std::set<std::type_index> registered{};
#endif

		[[nodiscard]] event_registry(const std::initializer_list<std::type_index> registeredList)
			noexcept(!DEBUG_CHECK)
#if DEBUG_CHECK
			: registered(registeredList)
#endif
		{}

		template <typename T>
		void checkRegister() const{
#if DEBUG_CHECK
			assert(registered.empty() || registered.contains(index_of<T>()));
#endif
		}

		template <typename... T>
		void registerType(){
#if DEBUG_CHECK
			(registered.insert(index_of<T>()), ...);
#endif
		}
	};


	template <template <typename...> typename Container = std::vector, typename Proj = EventProjection>
	struct BasicEventManager : event_registry{
	protected:
		using FuncType = std::function<void(const void*)>;
		using FuncContainer = Container<FuncType>;

		std::unordered_map<std::type_index, FuncContainer> events{};

	public:
		using event_registry::event_registry;

		template <std::derived_from<event_type> T>
			requires (std::is_final_v<T>)
		void fire(const T& event) const{
			checkRegister<T>();

			if(const auto itr = events.find(index_of<T>()); itr != events.end()){
				for(const auto& listener : Proj::projection(itr->second)){
					listener(&event);
				}
			}
		}

		template <std::derived_from<event_type> T, typename... Args>
			requires requires(Args&&... args){
				requires std::is_final_v<T>;
				T{std::forward<Args>(args)...};
			}
		void emplace_fire(Args&&... args) const{
			this->fire<T>(T{std::forward<Args>(args)...});
		}
	};

	export
	struct legacy_event_manager : BasicEventManager<std::vector>{
		// using BasicEventManager<std::vector>::BasicEventManager;
		template <std::derived_from<event_type> T, std::invocable<const T&> Func>
			requires std::is_final_v<T>
		void on(Func&& func){
			checkRegister<T>();

			events[index_of<T>()].emplace_back([fun = std::forward<decltype(func)>(func)](const void* event){
				fun(*static_cast<const T*>(event));
			});
		}
	};

	export
	struct LegacyNamedEventManager : BasicEventManager<string_hash_map, LegacyNamedEventManager>{
		// using BasicEventManager<StringHashMap, NamedEventManager>::BasicEventManager;

		template <std::derived_from<event_type> T, std::invocable<const T&> Func>
			requires std::is_final_v<T>
		void on(const std::string_view name, Func&& func){
			checkRegister<T>();

			events[index_of<T>()].insert_or_assign(name, [fun = std::forward<decltype(func)>(func)](const void* event){
				fun(*static_cast<const T*>(event));
			});
		}

		template <std::derived_from<event_type> T>
			requires std::is_final_v<T>
		std::optional<FuncType> erase(const std::string_view name){
			checkRegister<T>();

			if(const auto itr = events.find(index_of<T>()); itr != events.end()){
				if(const auto eitr = itr->second.find(name); eitr != itr->second.end()){
					std::optional opt{eitr->second};
					itr->second.erase(eitr);
					return opt;
				}
			}

			return std::nullopt;
		}

		static decltype(auto) projection(const FuncContainer& range){
			return range | std::views::values;
		}
	};

	/**
	 * @brief THE VALUE OF THE ENUM MEMBERS MUST BE CONTINUOUS
	 * @tparam T Used Enum Type
	 * @tparam maxsize How Many Items This Enum Contains
	 */
	export
	template <ext::enum_type T, std::underlying_type_t<T> maxsize>
	class signal_manager{
		std::array<std::vector<std::function<void()>>, maxsize> events{};

	public:
		using SignalType = std::underlying_type_t<T>;
		template <T index>
		static constexpr bool isValid = requires{
			requires static_cast<SignalType>(index) < maxsize;
			requires static_cast<SignalType>(index) >= 0;
		};

		[[nodiscard]] static constexpr SignalType max() noexcept{
			return maxsize;
		}

		void fire(const T signal){
			for(const auto& listener : events[std::to_underlying(signal)]){
				listener();
			}
		}

		template <std::invocable Func>
		void on(const T signal, Func&& func){
			events.at(std::to_underlying(signal)).push_back(std::function<void()>{std::forward<Func>(func)});
		}

		template <T signal, std::invocable Func>
			requires isValid<signal>
		void on(Func&& func){
			events.at(std::to_underlying(signal)).push_back(std::function<void()>{std::forward<Func>(func)});
		}

		template <T signal>
			requires isValid<signal>
		void fire(){
			for(const auto& listener : events[std::to_underlying(signal)]){
				listener();
			}
		}
	};

	template <typename T>
	concept non_const = !std::is_const_v<T>;

	export
	enum class CycleSignalState{
		begin, end,
	};
	export
	using CycleSignalManager = signal_manager<CycleSignalState, 2>;

	export
	template <bool allow_pmr = false>
	struct event_submitter : event_registry{
		std::unordered_map<std::type_index, std::vector<void*>> subscribers{};

		using event_registry::event_registry;

		template <non_const T, typename ArgT>
			requires requires{
				requires
				(!allow_pmr && std::convertible_to<ArgT, T> && std::same_as<std::remove_cv_t<ArgT>, std::remove_cv_t<T>>) ||
				(allow_pmr && std::derived_from<ArgT, T> && std::has_virtual_destructor_v<T>);
			}

		void submit(ArgT* ptr){
			checkRegister<std::decay_t<T>>();

			subscribers[index_of<T>()].push_back(static_cast<void*>(ptr));
		}

		template <non_const T, std::invocable<T&> Func>
			requires (!std::is_member_pointer_v<Func>)
		void invoke(Func&& func) const{
			checkRegister<std::decay_t<T>>();

			if(const auto itr = at<T>(); itr != subscribers.end()){
				for(auto second : itr->second){
					std::invoke(std::ref(std::as_const(func)), *static_cast<T*>(second));
				}
			}
		}

		template <non_const T, std::invocable<T*> Func>
		void invoke(Func&& func) const{
			checkRegister<std::decay_t<T>>();

			if(const auto itr = at<T>(); itr != subscribers.end()){
				for(auto second : itr->second){
					std::invoke(std::ref(std::as_const(func)), static_cast<T*>(second));
				}
			}
		}


		template <non_const T>
		void clear(){
			checkRegister<std::decay_t<T>>();

			if(const auto itr = at<T>(); itr != subscribers.end()){
				itr->second.clear();
			}
		}

		template <non_const T, std::invocable<T&> Func>
			requires (!std::is_member_pointer_v<Func>)
		void invoke_then_clear(Func&& func){
			checkRegister<std::decay_t<T>>();

			if(auto itr = at<T>(); itr != subscribers.end()){
				for(auto second : itr->second){
					std::invoke(std::ref(std::as_const(func)), *static_cast<T*>(second));
				}
				itr->second.clear();
			}
		}

		template <non_const T, std::invocable<T*> Func>
		void invoke_then_clear(Func&& func){
			checkRegister<std::decay_t<T>>();

			if(auto itr = at<T>(); itr != subscribers.end()){
				for(auto second : itr->second){
					std::invoke(std::ref(std::as_const(func)), static_cast<T*>(second));
				}
				itr->second.clear();
			}
		}

	private:
		template <typename T>
		decltype(subscribers)::const_iterator at() const{
			return subscribers.find(index_of<T>());
		}

		template <typename T>
		decltype(subscribers)::iterator at(){
			return subscribers.find(index_of<T>());
		}
	};
}


namespace ext{
	// using event_erased_func_type = void(const void*);
	// using T = std::add_const_t<event_erased_func_type>;

	export
	template <template<typename > typename T>
	using wrapped_func =
	T<std::conditional_t<
		std::same_as<T<void()>, std::move_only_function<void()>>,
		void(const void*) const,
		void(const void*)>>;
;
	template <
		template<typename > typename FuncWrapperTy = std::function,
		template <typename, typename...> typename Container = std::vector,
		std::indirectly_regular_unary_invocable<std::ranges::iterator_t<Container<wrapped_func<FuncWrapperTy>>>> Proj = std::identity,
		typename... EventTs>
	struct event_manager_base{

		static_assert(requires{
			typename wrapped_func<FuncWrapperTy>;
		}, "invalid function wrapper template type");

		using value_type = wrapped_func<FuncWrapperTy>;
		using container_type = std::vector<value_type>;
		using mapping_type = type_map<container_type, EventTs...>;

	protected:
		mapping_type events{};

		ADAPTED_NO_UNIQUE_ADDRESS Proj proj{};

	public:
		template <event_argument T>
			// requires (mapping_type::template is_type_valid<T>)
		const container_type& group_at() const noexcept{
			return events.template at<T>();
		}

		template <event_argument T>
			// requires (mapping_type::template is_type_valid<T>)
		container_type& group_at() noexcept{
			return events.template at<T>();
		}

	public:
		template <event_argument T>
			// requires (mapping_type::template is_type_valid<T>)
		void fire(const T& event) const{
			for (const wrapped_func<FuncWrapperTy>& listener : std::invoke(proj, group_at<T>())){
				//fuck msvc
				listener(const_cast<void*>(static_cast<const void*>(&event)));
			}
		}

	};

	export
	template<template<typename > typename FuncWrapperTy = std::function, typename... EventTs>
	struct event_manager : event_manager_base<FuncWrapperTy, std::vector, std::identity, EventTs...>{
	private:
		using base = event_manager_base<FuncWrapperTy, std::vector, std::identity, EventTs...>;

	public:
		template <event_argument T, std::invocable<const T&> Func>
		void on(Func&& func){
			std::vector<wrapped_func<FuncWrapperTy>>& tgt = this->template group_at<T>();

			tgt.emplace_back([fun = std::forward<decltype(func)>(func)](const void* event){
				fun(*static_cast<const T*>(event));
			});
		}
	};

	export
	template<template<typename > typename FuncWrapperTy = std::function, typename... EventTs>
	struct named_event_manager : event_manager_base<FuncWrapperTy, string_hash_map, decltype(std::views::values), EventTs...>{
		template <event_argument T, std::invocable<const T&> Func>
		void on(const std::string_view name, Func&& func){
			string_hash_map<wrapped_func<FuncWrapperTy>>& tgt = this->template group_at<T>();

			tgt.insert_or_assign(name, [fun = std::forward<Func>(func)](const void* event){
				fun(*static_cast<const T*>(event));
			});
		}

		template <event_argument T>
		std::optional<wrapped_func<FuncWrapperTy>> erase(const std::string_view name){
			string_hash_map<wrapped_func<FuncWrapperTy>>& tgt = this->template group_at<T>();

			if(const auto eitr = tgt.find(name); eitr != tgt.end()){
				std::optional opt{std::move(eitr->second)};
				tgt.erase(eitr);
				return opt;
			}

			return std::nullopt;
		}
	};
}

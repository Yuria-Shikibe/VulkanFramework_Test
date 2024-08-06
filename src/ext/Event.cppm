export module ext.Event;

import std;
import ext.Concepts;
import ext.Heterogeneous;

export namespace ext {
	struct EventType {};

	template<std::derived_from<EventType> T>
	constexpr std::type_index typeIndexOf() {
		return std::type_index(typeid(T));
	}

	struct EventProjection{
		static decltype(auto) projection(const std::ranges::range auto& range){
			return range;
		}
	};

	template <template <typename...> typename Container = std::vector, typename Proj = EventProjection>
	struct BasicEventManager{
	protected:
		using FuncType = std::function<void(const void*)>;
		using FuncContainer = Container<FuncType>;

#if DEBUG_CHECK
		std::set<std::type_index> registered{};
#endif
		std::unordered_map<std::type_index, FuncContainer> events{};


	public:
		template <std::derived_from<EventType> T>
			requires (std::is_final_v<T>)
		void fire(const T& event) const{
#if DEBUG_CHECK
			checkRegister<T>();
#endif
			if(const auto itr = events.find(typeIndexOf<T>()); itr != events.end()){
				for(const auto& listener : Proj::projection(itr->second)){
					listener(&event);
				}
			}
		}

		template <std::derived_from<EventType> T, typename... Args>
			requires std::is_final_v<T>
		void emplace_fire(Args&&... args) const{
			this->fire<T>(T{std::forward<Args>(args)...});
		}

		template <std::derived_from<EventType> ...T>
		void registerType(){
#if DEBUG_CHECK
			(registered.insert(typeIndexOf<T>()), ...);
#endif
		}

		[[nodiscard]] explicit BasicEventManager(std::set<std::type_index>&& registered)
#if DEBUG_CHECK
			: registered(std::move(registered))
#endif
		{}

		[[nodiscard]] BasicEventManager(const std::initializer_list<std::type_index> registeredList)
#if DEBUG_CHECK
			: registered(registeredList)
#endif
		{}

		[[nodiscard]] BasicEventManager() = default;

	protected:
		template <std::derived_from<EventType> T>
		void checkRegister() const{
#if DEBUG_CHECK
			if(!registered.empty() && !registered.contains(typeIndexOf<T>()))
				throw std::runtime_error{"Unexpected Event Type!"};
#endif
		}
	};

	struct EventManager : BasicEventManager<std::vector>{
		using BasicEventManager<std::vector>::BasicEventManager;
		template <std::derived_from<EventType> T, std::invocable<const T&> Func>
			requires std::is_final_v<T>
		void on(Func&& func){
#if DEBUG_CHECK
			checkRegister<T>();
#endif
			events[typeIndexOf<T>()].emplace_back([fun = std::forward<decltype(func)>(func)](const void* event){
				fun(*static_cast<const T*>(event));
			});
		}

	};

	struct NamedEventManager : BasicEventManager<ext::StringHashMap, NamedEventManager>{
		using BasicEventManager::BasicEventManager;

		template <std::derived_from<EventType> T, std::invocable<const T&> Func>
			requires std::is_final_v<T>
		void on(const std::string_view name, Func&& func){
#if DEBUG_CHECK
			checkRegister<T>();
#endif
			events[typeIndexOf<T>()].insert_or_assign(name, [fun = std::forward<decltype(func)>(func)](const void* event){
				fun(*static_cast<const T*>(event));
			});
		}

		template <std::derived_from<EventType> T>
			requires std::is_final_v<T>
		std::optional<FuncType> erase(const std::string_view name){
#if DEBUG_CHECK
			checkRegister<T>();
#endif
			if(const auto itr = events.find(typeIndexOf<T>()); itr != events.end()){
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
	template <Concepts::Enum T, std::underlying_type_t<T> maxsize>
	class SignalManager {
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

		void fire(const T signal) {
			for (const auto& listener : events[std::to_underlying(signal)]) {
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
			for (const auto& listener : events[std::to_underlying(signal)]) {
				listener();
			}
		}
	};


	enum class CycleSignalState {
		begin, end,
	};

	using CycleSignalManager = ext::SignalManager<ext::CycleSignalState, 2>;

	using enum CycleSignalState;
}


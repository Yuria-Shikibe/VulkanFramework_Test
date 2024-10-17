module;

#include <cassert>

export module Core.Ctrl.Bind:InputBind;

import Core.Ctrl.Constants;
import Core.Ctrl.KeyPack;
import std;
import ext.concepts;
import ext.algo;

export namespace Core::Ctrl{
	struct InputBind : UnpackedKey{
		// int expectedKey{Unknown};
		// int expectedAct{Act::Press};
		// int expectedMode{};

		std::function<void()> func{};

		[[nodiscard]] InputBind() = default;

		[[nodiscard]] InputBind(
			const int key, const int expectedState, const int expectedMode,
			const std::function<void()>& func)
			: UnpackedKey{key, expectedState, expectedMode},
			  func{func}{}

		[[nodiscard]] InputBind(const int button, const int expectedAct, const std::function<void()>& func)
			: InputBind{button, expectedAct, Mode::Ignore, func}{}

		[[nodiscard]] InputBind(const int button, const std::function<void()>& func) : InputBind(
			button, Act::Press, func){}

		[[nodiscard]] constexpr bool matched(const int act, const int mode) const noexcept{
			return Act::matched(act, this->act) && Mode::matched(mode, this->mode);
		}

		void tryExec(const int state, const int mode) const{
			if(matched(state, mode)) exec();
		}

		void exec() const{
			func();
		}

		friend constexpr bool operator==(const InputBind& lhs, const InputBind& rhs){
			return static_cast<const UnpackedKey&>(lhs) == static_cast<const UnpackedKey&>(rhs);
		}

	};
	
	class KeyMappingRegisterList{
		struct DoubleClickTimer{
			int key{};
			float time{};

			[[nodiscard]] DoubleClickTimer() = default;

			[[nodiscard]] explicit DoubleClickTimer(const int key)
				: key{key}, time{Act::doublePressMaxSpacing}{}
		};
		
		using Signal = unsigned short;
		/**
		 * @code
		 * | 0b'0000'0000'0000'0000
		 * |         CRrp|-- MOD --
		 * | C - Continuous
		 * | R - Repeat
		 * | r - release
		 * | p - press
		 * @endcode
		 */
		static constexpr Signal DoubleClick = 0b0001'0000'0000'0000;
		static constexpr Signal Continuous = 0b1000'0000'0000;
		static constexpr Signal Repeat = 0b0100'0000'0000;
		static constexpr Signal Release = 0b0010'0000'0000;
		static constexpr Signal Press = 0b0001'0000'0000;
		// static constexpr Signal RP_Eraser = ~(Release | Press | DoubleClick | Repeat);

		std::array<std::vector<InputBind>, AllKeyCount> binds{};
		std::array<std::vector<InputBind>, AllKeyCount> binds_continuous{};
		std::array<Signal, AllKeyCount> signals{};

		//OPTM using array instead of vector
		std::vector<Signal> pressed{};
		std::vector<DoubleClickTimer> doubleClickTimers{};

		void updateSignal(const int key, const int act){
			switch(act){
				case Act::Press :{
					if(!std::ranges::contains(pressed.crbegin(), pressed.crend(), key)){
						pressed.push_back(key);
					}
					break;
				}

				case Act::Release :{
					signals[key] = 0;
					ext::algo::erase_unique_unstable(pressed, key);
					break;
				}

				default : break;
			}
		}

		/**
		 * @return isDoubleClick
		 */
		bool insert(const int code, const int action, const int mode){
			Signal pushIn = signals[code] & ~Mode::Mask;

			bool isDoubleClick = false;

			switch(action){
				case Act::Press :{
					if(const auto itr = std::ranges::find(doubleClickTimers, code, &DoubleClickTimer::key); itr !=
						doubleClickTimers.end()){
						doubleClickTimers.erase(itr);
						isDoubleClick = true;
						pushIn = Press | Continuous | DoubleClick;
					} else{
						doubleClickTimers.emplace_back(code);
						pushIn = Press | Continuous;
					}

					break;
				}

				case Act::Release :{
					pushIn = Release;
					// pressed[code] &= ~Continuous;
					break;
				}

				case Act::Repeat : pushIn = Repeat | Continuous;
					break;

				default : break;
			}

			signals[code] = pushIn | (mode & Mode::Mask);

			return isDoubleClick;
		}

	public:
		[[nodiscard]] bool triggered(const int code, const int action, const int mode) const noexcept{
			const Signal target = signals[code];

			if(!Mode::matched(target, mode)) return false;

			Signal actionTgt{};
			switch(action){
				case Act::Press : actionTgt = Press;
					break;
				case Act::Continuous : actionTgt = Continuous;
					break;
				case Act::Release : actionTgt = Release;
					break;
				case Act::Repeat : actionTgt = Repeat;
					break;
				case Act::DoubleClick : actionTgt = DoubleClick;
					break;
				case Act::Ignore : actionTgt = Press | Continuous | Release | Repeat | DoubleClick;
					break;
				default : std::unreachable();
			}

			return target & actionTgt;
		}

		[[nodiscard]] int getMode() const noexcept{
			const auto shift = triggered(Key::Left_Shift) || triggered(Key::Right_Shift) ? Mode::Shift : Mode::None;
			const auto ctrl = triggered(Key::Left_Control) || triggered(Key::Right_Control) ? Mode::Ctrl : Mode::None;
			const auto alt = triggered(Key::Left_Alt) || triggered(Key::Right_Alt) ? Mode::Alt : Mode::None;
			const auto caps = triggered(Key::CapsLock) ? Mode::CapLock : Mode::None;
			const auto nums = triggered(Key::NumLock) ? Mode::NumLock : Mode::None;
			const auto super = triggered(Key::Left_Super) || triggered(Key::Right_Super) ? Mode::Super : Mode::None;

			return shift | ctrl | alt | caps | nums | super;
		}

		[[nodiscard]] bool triggered(const int code) const noexcept{
			return static_cast<bool>(signals[code]);
		}

		void inform(const int code, const int action, const int mods){
			const bool doubleClick = insert(code, action, mods);

			for(const auto& bind : binds[code]){
				bind.tryExec(action, mods);
				if(doubleClick){
					//TODO better double click event
					bind.tryExec(Act::DoubleClick, mods);
				}
			}

			updateSignal(code, action);
		}

		void update(const float delta){
			for(const int key : pressed){
				for(const auto& bind : binds_continuous[key]){
					bind.tryExec(Act::Continuous, signals[key] & Mode::Mask);
				}
			}

			std::erase_if(doubleClickTimers, [delta](DoubleClickTimer& timer){
				timer.time -= delta;
				return timer.time <= 0.f;
			});
		}

		void registerBind(InputBind&& bind){
			assert(bind.key < AllKeyCount && bind.key >= 0);
			
			auto& container = Act::isContinuous(bind.act) ? binds_continuous : binds;

			container[bind.key].emplace_back(std::move(bind));
		}

		void registerBind(const InputBind& bind){
			registerBind(InputBind{bind});
		}

		void clear() noexcept{
			std::ranges::for_each(binds, &std::vector<InputBind>::clear);
			std::ranges::for_each(binds_continuous, &std::vector<InputBind>::clear);
			signals.fill(0);
			pressed.clear();
			doubleClickTimers.clear();
		}
	};
	
	class KeyMapping{
		bool activated{true};

	public:
		KeyMappingRegisterList mapping{};

		void registerBind(InputBind&& bind){
			mapping.registerBind(std::move(bind));
		}

		template <std::invocable<> Func>
		void registerBind(const int key, const int expectedState, const int expectedMode, Func&& func){
			registerBind(InputBind{key, expectedState, expectedMode, std::forward<Func>(func)});
		}

		template <std::invocable<> Func>
		void registerBind(const int key, const int expectedState, Func&& func){
			registerBind(InputBind{key, expectedState, std::forward<Func>(func)});
		}

		template <std::invocable<> Func1, std::invocable<> Func2>
		void registerBind(const int key, const int expectedMode, Func1&& onPress, Func2&& onRelease){
			registerBind(InputBind{key, Act::Press, expectedMode, std::forward<Func1>(onPress)});
			registerBind(InputBind{
					key, Act::Release, expectedMode, std::forward<Func2>(onRelease)
				});
		}

		template <std::invocable<> Func>
		void registerBind(std::initializer_list<std::pair<int, int>> pairs, Func&& func){
			for(auto [key, expectedState] : pairs){
				registerBind(InputBind{key, expectedState, std::forward<Func>(func)});
			}
		}

		template <std::invocable<> Func>
		void registerBind(std::initializer_list<std::pair<int, int>> pairs, const int expectedMode, Func&& func){
			for(auto [key, expectedState] : pairs){
				registerBind(InputBind{key, expectedState, expectedMode, std::forward<Func>(func)});
			}
		}

		template <std::invocable<> Func>
		void registerBind(std::initializer_list<UnpackedKey> params, Func&& func){
			for(auto [key, expectedState, expectedMode] : params){
				registerBind(InputBind{key, expectedState, expectedMode, std::forward<Func>(func)});
			}
		}

		[[nodiscard]] bool isPressedKey(const int key) const{
			return mapping.triggered(key);
		}

		void clear(){
			mapping = {};
		}

		void update(const float delta){
			if(!activated) return;

			mapping.update(delta);
		}
		//
		// void informMouseAction(const int key, const int action, const int mods){
		// 	if(activated) keyMapping.inform(key, action, mods);
		// }
		//
		// void informKeyAction(const int key, const int action, const int mods){
		// 	if(activated) keyMapping.inform(key, action, mods);
		// }

		void setActivated(const bool activated) noexcept{ this->activated = activated; }

		[[nodiscard]] bool isActivated() const noexcept{
			return activated;
		}

		void activate() noexcept{
			activated = true;
		}

		void deactivate() noexcept{
			activated = false;
		}
	};
}

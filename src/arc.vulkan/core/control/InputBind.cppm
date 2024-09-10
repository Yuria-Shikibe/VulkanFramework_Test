export module Core.Ctrl.Bind:InputBind;

import Core.Ctrl.Bind.Constants;
import std;
import ext.concepts;

export namespace Core::Ctrl{
	struct InputBind{
	protected:
		int key = Unknown;
		int expectedAct = Act::Press;
		int expectedMode = 0;
		bool ignoreMode = true;

		std::function<void()> action = nullptr;

	public:
		[[nodiscard]] InputBind() = default;

		[[nodiscard]] InputBind(const int key, const int expectedState, const int expectedMode,
		                            const bool ignoreMode,
		                            const std::function<void()>& action)
			: key{key},
			  expectedAct{expectedState},
			  expectedMode{expectedMode},
			  ignoreMode{ignoreMode},
			  action{action}{}

		[[nodiscard]] InputBind(const int button, const int expected_state, const int expected_mode,
		                            const std::function<void()>& action)
			: InputBind(button, expected_state, expected_mode, expected_mode == Mode::Ignore, action){}

		[[nodiscard]] InputBind(const int button, const int expected_state, const std::function<void()>& action)
			: InputBind{button, expected_state, Mode::Ignore, action}{}

		[[nodiscard]] InputBind(const int button, const std::function<void()>& action) : InputBind(
			button, Act::Press, action){}


		[[nodiscard]] constexpr int getKey() const noexcept{
			return key;
		}

		[[nodiscard]] bool isIgnoreMode() const{ return ignoreMode; }

		void setIgnoreMode(const bool ignoreMode){ this->ignoreMode = ignoreMode; }

		void setKey(const int key){ this->key = key; }

		void setState(const int expectedAct){ this->expectedAct = expectedAct; }

		void setMode(const int expectedMode){ this->expectedMode = expectedMode; }

		[[nodiscard]] constexpr int getFullKey() const noexcept{
			return Ctrl::getFullKey(key, expectedAct, expectedMode);
		}

		[[nodiscard]] constexpr int state() const noexcept{
			return expectedAct;
		}

		[[nodiscard]] constexpr int mode() const noexcept{
			return expectedMode;
		}

		[[nodiscard]] constexpr bool activated(const int state, const int mode) const noexcept{
			return expectedAct == state && modeMatch(mode);
		}

		[[nodiscard]] constexpr bool modeMatch(const int mode) const noexcept{
			return ignoreMode || Mode::modeMatch(mode, expectedMode);
		}

		void tryRun(const int state, const int mode) const {
			if (activated(state, mode))act();
		}

		void act() const {
			action();
		}

		friend constexpr bool operator==(const InputBind& lhs, const InputBind& rhs) {
			return lhs.key == rhs.key && lhs.expectedAct == rhs.expectedAct && &lhs.action == &rhs.action;
		}

		friend constexpr bool operator!=(const InputBind& lhs, const InputBind& rhs) {
			return !(lhs == rhs);
		}
	};

	class InputBindGroup{
		class InputGroup{
			using Signal = int;
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
			static constexpr Signal ModeMask = 0xff;
			static constexpr Signal DoubleClick = 0b0001'0000'0000'0000;
			static constexpr Signal Continuous = 0b1000'0000'0000;
			static constexpr Signal Repeat = 0b0100'0000'0000;
			static constexpr Signal Release = 0b0010'0000'0000;
			static constexpr Signal Press = 0b0001'0000'0000;
			static constexpr Signal RP_Eraser = ~(Release | Press | DoubleClick);

			std::array<std::vector<InputBind>, AllKeyCount> binds{};
			std::array<std::vector<InputBind>, AllKeyCount> continuous{};
			std::array<float, AllKeyCount> doubleClick{};
			std::array<unsigned short, AllKeyCount> pressed{};

		    //Just uses vector??
			std::unordered_set<Signal> markedSignal{};

		private:
			void updateSignal(const int code){
				if(code == Act::Release){
					markedSignal.erase(code);
				}else{
					markedSignal.insert(code);
				}
			}

			/**
			 * @return isDoubleClick
			 */
			bool insert(const int code, const int action, const int mode){
				Signal pushIn = pressed[code] & ~ModeMask;

				bool isDoubleClick = false;

				switch(action){
					case Act::Press :{
						if(doubleClick[code] <= 0){
							doubleClick[code] = doublePressMaxSpaceing;
							pushIn = Press | Continuous;
						} else{
							doubleClick[code] = -1;
							isDoubleClick = true;
							pushIn = Press | DoubleClick;
						}

						break;
					}
					case Act::Release : pushIn = Release;
						break;
					case Act::Repeat : pushIn = Repeat | Continuous;
						break;
					default : break;
				}

				pressed[code] = pushIn | (mode & ModeMask);

				return isDoubleClick;
			}

		public:
			[[nodiscard]] bool get(const int code, const int action, const int mode) const{
				Signal actionTgt{};
				const Signal target = pressed[code];

				if(mode != Mode::Ignore){
					if((target & ModeMask) != (mode & ModeMask)) return false;
				}

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
					default : break;
				}

				return target & actionTgt;
			}

			[[nodiscard]] bool hasAction(const int code) const{
				return static_cast<bool>(pressed[code]);
			}

			void inform(const int code, const int action, const int mods){
				updateSignal(code);
				const bool doubleClick = insert(code, action, mods);

				const auto& targets = binds[code];

				if(!targets.empty()){
					for(const auto& bind : targets){
						bind.tryRun(action, mods);
						if(doubleClick){
							//TODO better double click event
							bind.tryRun(Act::DoubleClick, mods);
						}
					}
				}
			}

			void update(const float delta){
				for(const int key : markedSignal){
					pressed[key] &= RP_Eraser;
					if(pressed[key] & Continuous){
						for(const auto& bind : continuous[key]){
							bind.tryRun(Act::Continuous, pressed[key]);
						}
					}
				}

				for(float& reload : doubleClick){
					if(reload > 0) reload -= delta;
				}
			}

			void registerBind(InputBind&& bind){
				if(bind.getKey() >= AllKeyCount)return;
				auto& container = isContinuous(bind.state()) ? continuous : binds;

				container[bind.getKey()].emplace_back(std::move(bind));
			}

			void clear(){
				std::ranges::for_each(binds, &std::vector<InputBind>::clear);
				std::ranges::for_each(continuous, &std::vector<InputBind>::clear);
			}
		};

		bool activated{true};

	public:
		InputGroup registerList{};

		void registerBind(InputBind&& bind){
			registerList.registerBind(std::move(bind));
		}

		template <ext::Invokable<void()> Func>
		void registerBind(const int key, const int expectedState, const int expectedMode, Func&& func){
			registerBind(InputBind{key, expectedState, expectedMode, std::forward<Func>(func)});
		}

		template <ext::Invokable<void()> Func>
		void registerBind(const int key, const int expectedState, Func&& func){
			registerBind(InputBind{key, expectedState, std::forward<Func>(func)});
		}

		template <ext::Invokable<void()> Func1, ext::Invokable<void()> Func2>
		void registerBind(const int key, const int expectedMode, Func1&& onPress, Func2&& onRelease){
			registerBind(InputBind{key, Ctrl::Act::Press, expectedMode, std::forward<Func1>(onPress)});
			registerBind(InputBind{
					key, Ctrl::Act::Release, expectedMode, std::forward<Func2>(onRelease)
				});
		}

		template <ext::Invokable<void()> Func>
		void registerBind(std::initializer_list<std::pair<int, int>> pairs, Func&& func){
			for(auto [key, expectedState] : pairs){
				registerBind(InputBind{key, expectedState, std::forward<Func>(func)});
			}
		}

		template <ext::Invokable<void()> Func>
		void registerBind(std::initializer_list<std::pair<int, int>> pairs, const int expectedMode, Func&& func){
			for(auto [key, expectedState] : pairs){
				registerBind(InputBind{key, expectedState, expectedMode, std::forward<Func>(func)});
			}
		}

		template <ext::Invokable<void()> Func>
		void registerBind(std::initializer_list<std::tuple<int, int, int>> params, Func&& func){
			for(auto [key, expectedState, expectedMode] : params){
				registerBind(InputBind{key, expectedState, expectedMode, std::forward<Func>(func)});
			}
		}

		[[nodiscard]] bool isPressedKey(const int key) const{
			return registerList.hasAction(key);
		}

		void clearAllBinds(){
			registerList = InputGroup{};
		}

		void update(const float delta){
			if(!activated) return;

			registerList.update(delta);
		}

		/**
		 * \brief No remove function provided. If the input binds needed to be changed, clear it all and register the new binds by the action table.
		 * this is an infrequent operation so just keep the code clean.
		 */
		void informMouseAction(const int key, const int action, const int mods){
			if(activated) registerList.inform(key, action, mods);
		}

		void informKeyAction(const int key, const int action, const int mods){
			if(activated) registerList.inform(key, action, mods);
		}

		void setActivated(const bool activated) noexcept{ this->activated = activated; }

		[[nodiscard]] bool isActivated() const noexcept{ return activated; }

		void activate() noexcept{ activated = true; }

		void deactivate() noexcept{ activated = false; }
	};

}

//
// Created by Matrix on 2024/9/24.
//

export module Core.UI.Button;

import Core.UI.Element;
import Core.Ctrl.KeyPack;
import std;

namespace Core::UI{
	template <typename T>
	struct Unsupported : std::false_type{};

	namespace ButtonTag{
		export enum TagEnum : unsigned{
			none = 0,
			forwardKey = 1 << 0,
			// onReleaseOnly = 1 << 1,
			// forward = std::bit_ceil(static_cast<unsigned>(Ctrl::Mouse::Count)),
			// onReleaseOnly = forward << 1,
		};

		export
		template <TagEnum Tag, Ctrl::KeyPack specKeyPack>
		struct InvokeTag{
			static constexpr auto code = specKeyPack;
			static constexpr TagEnum tag = Tag;
		};

		export {
			constexpr auto Forward = InvokeTag<forwardKey, Ctrl::KeyPack{0, Ctrl::Act::Ignore, Ctrl::Mode::Ignore}>{};
			constexpr auto Normal = InvokeTag<forwardKey, Ctrl::KeyPack{0, Ctrl::Act::Release, Ctrl::Mode::Ignore}>{};
		}
	}

	struct T{
		int value;
	};

	struct T2 :T{
		void foo(){
			T::value = 1;
		}
	};


	export
	template <std::derived_from<Element> T>
	struct Button : public T{
	protected:
		using CallbackType = std::move_only_function<void(Button<T>&, Event::Click)>;
		CallbackType callback{};

	public:
		[[nodiscard]] Button(){
			Element::cursorState.registerDefEvent(Element::events());

			Element::events().template on<Event::Click>([this](const Event::Click& e){
				if(callback && Element::containsPos(e.pos)) std::invoke(callback, *this, e);
			});

			Element::interactivity = Interactivity::enabled;
			Element::property.maintainFocusUntilMouseDrop = true;
		}

		void setCallback(CallbackType&& func){
			callback = std::move(func);
		}

	private:
		template <ButtonTag::TagEnum tag, Ctrl::KeyPack keyPack, std::invocable<Button&, Event::Click> Func>
		decltype(auto) assignTagToFunc(Func&& func){
			if constexpr(tag & ButtonTag::forwardKey){
				if constexpr(keyPack.action() == Ctrl::Act::Ignore && keyPack.mode() == Ctrl::Mode::Ignore){
					return std::forward<Func>(func);
				} else{
					return [func = std::forward<Func>(func)](Button& b, const Event::Click e){
						if(Ctrl::Act::matched(e.code.action(), keyPack.action()) && Ctrl::Mode::matched(e.code.mode(), keyPack.mode())){
							std::invoke(func, b, e);
						}
					};
				}
			} else{
				return [func = std::forward<Func>(func)](Button& b, const Event::Click e){
					if(e.code.matched(keyPack)){
						std::invoke(func, b, e);
					}
				};
			}
		}

	public:
		template <ButtonTag::TagEnum tag, Ctrl::KeyPack keyPack, typename Func>
		void setCallback(ButtonTag::InvokeTag<tag, keyPack>, Func&& callback){

			if constexpr(std::invocable<Func, Button&, Event::Click> && std::assignable_from<CallbackType, Func>){
				this->callback = Button::assignTagToFunc<tag, keyPack>(std::forward<Func>(callback));
			}else if constexpr (std::invocable<Func, Event::Click>){
				this->callback = Button::assignTagToFunc<tag, keyPack>([func = std::forward<Func>(callback)](Button&, const Event::Click e){
					std::invoke(func, e);
				});
			}else if constexpr (std::invocable<Func>){
				this->callback = Button::assignTagToFunc<tag, keyPack>([func = std::forward<Func>(callback)](Button&, const Event::Click e){
					std::invoke(func);
				});
			}else{
				static_assert(Unsupported<Func>::value, "unsupported function type");
			}
		}
	};

	void foo(){
		Button<Element> button;
		button.setCallback(
			ButtonTag::InvokeTag<ButtonTag::none, Ctrl::KeyPack{Ctrl::Mouse::LMB}>{},
			[](Event::Click){

			}
		);
	}

}

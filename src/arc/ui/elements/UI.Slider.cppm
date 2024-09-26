//
// Created by Matrix on 2024/9/24.
//

export module Core.UI.Slider;

export import Core.UI.Element;
import Core.Ctrl.KeyPack;

import ext.snap_shot;

import Math;
import std;

export namespace Core::UI{
	class Slider : public Element{
	protected:
		/**
		 * @brief Has 2 degree of freedom [x, y]
		 */
		ext::snap_shot<Geom::Vec2> barProgress{};

		std::move_only_function<void(Slider&)> callback{};

		[[nodiscard]] Geom::Vec2 getBarMovableSize() const{
			return prop().getValidSize() - barBaseSize;
		}

		[[nodiscard]] Geom::Vec2 getSegmentUnit() const{
			return getBarMovableSize() / segments.copy().max({1, 1}).as<float>();
		}

		void moveBar(const Geom::Vec2 baseMovement) noexcept{
			if(isSegmentMoveActivated()){
				barProgress.temp =
					(barProgress.base + (baseMovement * sensitivity).roundBy(getSegmentUnit()) / getBarMovableSize()).clampNormalized();
			} else{
				barProgress.temp = (barProgress.base + (baseMovement * sensitivity) / getBarMovableSize()).clampNormalized();
			}
		}

		void applyLast(){
			if(barProgress.try_apply()){
				if(callback)callback(*this);
			}
		}

		void resumeLast(){
			barProgress.resume();
		}

	public:
		/**
		 * @brief set 0 to disable one freedom degree
		 * Negative value is accepted to flip the operation
		 */
		Geom::Vec2 sensitivity{1.0f, 1.0f};

		/**
		 * @brief Negative value is accepted to flip the operation
		 */
		Geom::Vec2 scrollSensitivity{6.0f, 3.0f};

		Geom::Vec2 barBaseSize{10.0f, 10.0f};
		Geom::Point2U segments{};

		Slider() : Element{"Slider"}{
			cursorState.registerDefEvent(events());

			events().on<Event::Click>([this](const Event::Click& event){
				const auto [key, action, mode] = event.unpack();

				switch(action){
					case Ctrl::Act::Press :{
						if(mode == Ctrl::Mode::Ctrl){
							const auto move =
								event.pos - (getProgress() * getValidSize() + absPos() +
									property.boarder.bot_lft());
							moveBar(move);
							applyLast();
						}
						break;
					}

					case Ctrl::Act::Release :{
						applyLast();
					}

					default : break;
				}

			});

			events().on<Event::Scroll>([this](const Event::Scroll& event){
				Geom::Vec2 move = event.pos;

				if(isClamped()){
					moveBar(scrollSensitivity * sensitivity.normalizeToBase() * move.y * Geom::Vec2{-1, 1});
				} else{
					if(isSegmentMoveActivated()){
						move.normalizeToBase().mul(getSegmentUnit());
					} else{
						move *= scrollSensitivity;
					}
					moveBar(move);
				}

				applyLast();
			});

			events().on<Event::Drag>([this](const Event::Drag& event){
				moveBar(event.trans());
			});

			events().on<Event::Exbound>([this](const auto& event){
				setFocusedScroll(false);
			});

			events().on<Event::Inbound>([this](const auto& event){
				setFocusedScroll(true);
			});

			property.maintainFocusUntilMouseDrop = true;
		}

		template <std::invocable<Slider&> Func>
		void setCallback(Func&& func){
			callback = std::forward<Func>(func);
		}

		void setInitialProgress(const Geom::Vec2 progress) noexcept{
			this->barProgress.base = progress;
		}

		[[nodiscard]] bool isSegmentMoveActivated() const noexcept{
			return static_cast<bool>(segments.x) || static_cast<bool>(segments.y);
		}

		[[nodiscard]] bool isClamped() const noexcept{
			return isClampedOnHori() || isClampedOnVert();
		}

		[[nodiscard]] Geom::Vec2 getBarLastPos() const noexcept{
			return getBarMovableSize() * barProgress.temp;
		}

		[[nodiscard]] Geom::Vec2 getBarCurPos() const noexcept{
			return getBarMovableSize() * barProgress.base;
		}

		[[nodiscard]] Geom::Vec2 getProgress() const noexcept{
			return barProgress.base;
		}

		void setHoriOnly() noexcept{ sensitivity.y = 0.0f; }

		[[nodiscard]] bool isClampedOnHori() const noexcept{ return sensitivity.y == 0.0f; }

		void setVertOnly() noexcept{ sensitivity.x = 0.0f; }

		[[nodiscard]] bool isClampedOnVert() const noexcept{ return sensitivity.x == 0.0f; }


		void drawMain() const override;

		[[nodiscard]] constexpr Geom::Vec2 getBarSize() const noexcept{
			return {
				isClampedOnVert() ? property.getValidWidth() : barBaseSize.x,
				isClampedOnHori() ? property.getValidHeight() : barBaseSize.y,
			};
		}

		// CursorType getCursorType() const noexcept override{
		// 	if(pressed) return CursorType::drag;
		//
		// 	if(isClampedOnHori()) return CursorType::scrollHori;
		// 	if(isClampedOnVert()) return CursorType::scrollVert;
		//
		// 	return CursorType::scroll;
		// }

		// void applyDefDrawer() override;
		//
		// void drawContent() const override;
	};
}

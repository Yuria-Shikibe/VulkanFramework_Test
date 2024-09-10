// ReSharper disable CppHidingFunction
export module Core.Input;

import std;

import Geom.Vector2D;
import ext.concepts;
import Core.Ctrl.Bind.Constants;
import Core.Ctrl.Bind;
import Core.InputListener;

export namespace Core::Ctrl{

	class Input{
	public:
		using PosListener = std::function<void(float, float)>;
		std::vector<PosListener> scrollListeners{};
		std::vector<PosListener> cursorMoveListeners{};
		std::vector<PosListener> velocityListeners{};

		std::vector<InputListener*> inputKeyListeners{};
		std::vector<InputListener*> inputMouseListeners{};

		InputBindGroup binds{};

	protected:
		bool isInbound{false};

		Geom::Vec2 mousePos{};
		Geom::Vec2 lastMousePos{};
		Geom::Vec2 mouseVelocity{};
		Geom::Vec2 scrollOffset{};

		std::vector<InputBindGroup*> subInputs{};

	public:
		/**
		 * @return false if Notfound
		 */
		bool activeBinds(const InputBindGroup* binds){
			if(const auto itr = std::ranges::find(subInputs, binds); itr != subInputs.end()){
				itr.operator*()->activate();
				return true;
			}
			return false;
		}

		/**
		* @return false if Notfound
		*/
		bool deactiveBinds(const InputBindGroup* binds){
			if(const auto itr = std::ranges::find(subInputs, binds); itr != subInputs.end()){
				itr.operator*()->deactivate();
				return true;
			}
			return false;
		}

		void registerSubInput(InputBindGroup* input){
			subInputs.push_back(input);
		}

		void eraseSubInput(InputBindGroup* input){
			std::erase(subInputs, input);
		}

		/**
		 * \brief No remove function provided. If the input binds needed to be changed, clear it all and register the new binds by the action table.
		 * this is an infrequent operation so just keep the code clean.
		 */
		void informMouseAction(const int button, const int action, const int mods){
			binds.informMouseAction(button, action, mods);

			for(const auto& listener : inputMouseListeners){
				listener->inform(button, action, mods);
			}

			for(auto* subInput : subInputs){
				subInput->informMouseAction(button, action, mods);
			}
		}

		void informKeyAction(const int key, const int scanCode, const int action, const int mods){
			binds.informKeyAction(key, action, mods);

			for(const auto& listener : inputKeyListeners){
				listener->inform(key, action, mods);
			}

			for(auto* subInput : subInputs){
				subInput->informKeyAction(key, action, mods);
			}
		}

		void cursorMoveInform(const float x, const float y){
			mousePos.set(x, y);

			for(auto& listener : cursorMoveListeners){
				listener(x, y);
			}
		}

		[[nodiscard]] Geom::Vec2 getCursorPos() const{
			return mousePos;
		}

		[[nodiscard]] Geom::Vec2 getScrollOffset() const{
			return scrollOffset;
		}

		void setScrollOffset(const float x, const float y){
			scrollOffset.set(-x, y);

			for(auto& listener : scrollListeners){
				listener(-x, y);
			}
		}

		[[nodiscard]] bool cursorInbound() const{
			return isInbound;
		}

		void setInbound(const bool b){
			isInbound = b;
		}

		void update(const float delta){
			binds.update(delta);

			mouseVelocity = mousePos;
			mouseVelocity -= lastMousePos;
			mouseVelocity /= delta;

			lastMousePos = mousePos;

			for(auto& listener : velocityListeners){
				listener(mouseVelocity.x, mouseVelocity.y);
			}

			for(auto* subInput : subInputs){
				subInput->update(delta);
			}
		}
	};
}

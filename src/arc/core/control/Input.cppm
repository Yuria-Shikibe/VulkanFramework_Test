// ReSharper disable CppHidingFunction
export module Core.Input;

import std;

import Geom.Vector2D;
import ext.concepts;
import ext.heterogeneous;
import Core.Ctrl.Constants;
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

		KeyMapping binds{};

	protected:
		bool isInbound{false};

		Geom::Vec2 mousePos{};
		Geom::Vec2 lastMousePos{};
		Geom::Vec2 mouseVelocity{};
		Geom::Vec2 scrollOffset{};

		ext::string_hash_map<KeyMapping> subInputs{};

	public:

		KeyMapping& registerSubInput(const std::string_view mappingName){
			return subInputs.try_emplace(std::string(mappingName), KeyMapping{}).first->second;
		}

		bool eraseSubInput(const std::string_view mappingName){
			return subInputs.erase(mappingName);
		}

		void inform(const int button, const int action, const int mods){
			binds.mapping.inform(button, action, mods);

			for(auto& subInput : subInputs | std::views::values){
				subInput.mapping.inform(button, action, mods);
			}
		}

		/**
		 * \brief No remove function provided. If the input binds needed to be changed, clear it all and register the new binds by the action table.
		 * this is an infrequent operation so just keep the code clean.
		 */
		void informMouseAction(const int button, const int action, const int mods){
			for(const auto& listener : inputMouseListeners){
				listener->inform(button, action, mods);
			}

			inform(button, action, mods);
		}

		void informKeyAction(const int key, const int scanCode, const int action, const int mods){
			for(const auto& listener : inputKeyListeners){
				listener->inform(key, action, mods);
			}

			inform(key, action, mods);
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

			for(auto& subInput : subInputs | std::views::values){
				subInput.update(delta);
			}
		}
	};
}

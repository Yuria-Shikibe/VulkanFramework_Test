//
// Created by Matrix on 2024/10/1.
//

export module Core.UI.Scene;

export import Core.UI.ToolTipManager;

import Geom.Vector2D;
import Geom.Rect_Orthogonal;
import Core.Ctrl.Constants;

import ext.owner;
import ext.handle_wrapper;

import std;

export namespace Core{
	class Bundle;
}

export namespace Graphic{
	struct RendererUI;
}

namespace Core::UI{
	struct SceneBase{
		struct MouseState{
			Geom::Vec2 src{};
			bool pressed{};

			void reset(const Geom::Vec2 pos){
				src = pos;
				pressed = true;
			}

			void clear(const Geom::Vec2 pos){

				src = pos;
				pressed = false;
			}
		};

		Geom::Vec2 pos{};
		Geom::Vec2 size{};

		Geom::Vec2 cursorPos{};
		std::array<MouseState, Ctrl::Mouse::Count> mouseKeyStates{};


		ext::dependency<ext::owner<Group*>> root{};

		//focus fields
		// Element* currentInputFocused{nullptr};
		Element* currentScrollFocus{nullptr};
		Element* currentCursorFocus{nullptr};

		std::vector<Element*> lastInbounds{};
		std::unordered_set<Element*> independentLayout{};
		std::unordered_set<Element*> asyncTaskOwners{};

		Graphic::RendererUI* renderer{};
		Bundle* bundle{};

		[[nodiscard]] SceneBase() = default;

		[[nodiscard]] SceneBase(const ext::owner<Group*> root, Graphic::RendererUI* renderer, Bundle* bundle)
			: root{root},
			  renderer{renderer},
			  bundle{bundle}{}

		SceneBase(const SceneBase& other) = delete;

		SceneBase(SceneBase&& other) noexcept = default;

		SceneBase& operator=(const SceneBase& other) = delete;

		SceneBase& operator=(SceneBase&& other) noexcept = default;
	};

	export struct Scene : SceneBase{
		ToolTipManager tooltipManager{};

		[[nodiscard]] Scene() = default;

		[[nodiscard]] explicit Scene(
			ext::owner<Group*> root,
			Graphic::RendererUI* renderer = nullptr,
			Bundle* bundle = nullptr);

		~Scene();

		[[nodiscard]] Geom::Rect_Orthogonal<float> getBound() const noexcept{
			return Geom::Rect_Orthogonal<float>{size};
		}

		void registerAsyncTaskElement(Element* element);

		void registerIndependentLayout(Element* element);

		[[nodiscard]] bool isMousePressed() const noexcept{
			return std::ranges::any_of(mouseKeyStates, std::identity{}, &MouseState::pressed);
		}

		void joinTasks();

		void dropAllFocus(const Element* target);

		void trySwapFocus(Element* newFocus);

		void swapFocus(Element* newFocus);

		void onMouseAction(int key, int action, int mode);

		void onKeyAction(int key, int action, int mode) const;

		void onTextInput(int code, int mode);

		void onScroll(Geom::Vec2 scroll) const;

		void onCursorPosUpdate(Geom::Vec2 newPos);

		void resize(Geom::Vec2 size);

		void update(float delta_in_ticks);

		void layout();

		void draw() const;

		Scene(const Scene& other) = delete;

		Scene(Scene&& other) noexcept;

		Scene& operator=(const Scene& other) = delete;

		Scene& operator=(Scene&& other) noexcept;

	private:
		void updateInbounds(std::vector<Element*>&& next);
	};


}

//
// Created by Matrix on 2024/9/14.
//

export module Core.UI.Root;

export import Core.UI.Element;

import Geom.Vector2D;

import ext.heterogeneous;
import std;

export namespace Core::UI{
	namespace SceneName{
		constexpr std::string_view Main{"main"};
	}

	struct Root{
		[[nodiscard]] Root() = default;

		[[nodiscard]] explicit Root(Scene&& scene){
			focus = &scenes.insert_or_assign(SceneName::Main, std::move(scene)).first->second;
		}

		ext::string_hash_map<Scene> scenes{};

		Scene* focus{};

		void draw() const{
			if(focus)focus->draw();
		}

		void update(const float delta_in_ticks) const{
			if(focus)focus->update(delta_in_ticks);
		}

		void layout() const{
			if(focus)focus->layout();
		}

		void inputKey(const int key, const int action, const int mode) const{
			if(focus)focus->onKeyAction(key, action, mode);
		}

		void inputScroll(const float x, const float y) const{
			if(focus)focus->onScroll({x, y});
		}

		void inputMouse(const int key, const int action, const int mode) const{
			if(focus)focus->onMouseAction(key, action, mode);
		}

		void cursorPosUpdate(const float x, const float y) const{
			if(focus)focus->onCursorPosUpdate({x, y});
		}

		template <typename T = Group>
		T& rootOf(const std::string_view sceneName){
			if(const auto rst = scenes.try_find(sceneName)){
				return dynamic_cast<T&>(*rst->root);
			}

			std::println(std::cerr, "Scene {} Not Found", sceneName);
			throw std::invalid_argument{"In-exist Scene Name"};
		}

		void resize(const Geom::Vec2 size, const std::string_view name = SceneName::Main){
			if(const auto rst = scenes.try_find(name)){
				rst->resize(size);
			}
		}

		[[nodiscard]] bool scrollIdle() const noexcept{
			return !focus || focus->currentScrollFocus == nullptr;
		}

		[[nodiscard]] bool focusIdle() const noexcept{
			return !focus || focus->currentCursorFocus == nullptr;
		}
	};
}

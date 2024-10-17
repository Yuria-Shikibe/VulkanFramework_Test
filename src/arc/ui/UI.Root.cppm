module;

#include <cassert>

export module Core.UI.Root;

export import Core.UI.Scene;

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
			auto name = std::string(scene.name);
			focus = &scenes.insert_or_assign(std::move(name), std::move(scene)).first->second;
		}

		ext::string_hash_map<Scene> scenes{};

		Scene* focus{};

		void draw() const{
			assert(focus != nullptr);
			focus->draw();
		}

		void update(const float delta_in_ticks) const{
			assert(focus != nullptr);
			focus->update(delta_in_ticks);
		}

		void layout() const{
			assert(focus != nullptr);
			focus->layout();
		}

		void inputKey(const int key, const int action, const int mode) const{
			assert(focus != nullptr);
			focus->onKeyAction(key, action, mode);
		}

		void inputScroll(const float x, const float y) const{
			assert(focus != nullptr);
			focus->onScroll({x, y});
		}

		void inputMouse(const int key, const int action, const int mode) const{
			assert(focus != nullptr);
			focus->onMouseAction(key, action, mode);
		}

		void cursorPosUpdate(const float x, const float y) const{
			assert(focus != nullptr);
			focus->onCursorPosUpdate({x, y});
		}

		template <std::derived_from<Group> T = Group>
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

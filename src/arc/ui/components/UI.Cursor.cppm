//
// Created by Matrix on 2024/4/15.
//

export module Core.UI.Cursor;

import Geom.Vector2D;
import std;

namespace Core::UI::Cursor{
	export
	enum struct CursorType : unsigned {
		regular,
		clickable,
		regular_tip,
		clickable_tip,

		select,
		select_regular,

		textInput,
		scroll,
		scrollHori,
		scrollVert,
		drag,
		//...
		count,
	};

	export
	struct CursorAdditionalDrawer {
		virtual ~CursorAdditionalDrawer() = default;
		virtual void operator()(float x, float y, float w, float h) = 0;
	};

	//TODO no nested virtual in virtual
	export
	struct CursorDrawable{
		virtual ~CursorDrawable() = default;

		std::unique_ptr<CursorAdditionalDrawer> drawer{nullptr};

		virtual void draw(float x, float y, Geom::Vec2 screenSize, float progress, float scl) const = 0;

		void draw(const float x, const float y, const Geom::Vec2 screenSize, const float progress = 0.0f) const{
			draw(x, y, screenSize, progress, 1.0f);
		}
	};

	struct EmptyCursor final : CursorDrawable{
		void draw(float x, float y, Geom::Vec2 screenSize, float progress, float scl) const override{

		}
	};

	constexpr EmptyCursor EmptyCursor{};

	//TODO is unordered_map better for extension?
	inline std::array<std::unique_ptr<CursorDrawable>, std::to_underlying(CursorType::count)> Cursors{};

	export{
		const CursorDrawable& get(const CursorType type){
			if(const auto& ptr = Cursors[std::to_underlying(type)])return *ptr;
			return EmptyCursor;
		}

		CursorDrawable& at(const CursorType type){
			return *Cursors[std::to_underlying(type)];
		}

		std::unique_ptr<CursorDrawable>& getRaw(CursorType type){
			return Cursors[std::to_underlying(type)];
		}

		void set(const CursorType type, std::unique_ptr<CursorDrawable>&& cursor){
			const auto s = std::to_underlying(type);
			Cursors.at(s) = std::move(cursor);
		}
	}
}

module;

#include <cassert>

export module Core.UI.Group;

export import Core.UI.Element;

import std;

export namespace Core::UI{
	struct Group : public Element{
		using Element::Element;

		virtual void postRemove(Element* element) = 0;

		virtual void instantRemove(Element* element) = 0;

		virtual Element& addChildren(ElementUniquePtr&& element) = 0;
		//TODO using explicit this


		virtual void updateChildren(const float delta_in_ticks){
			for(const auto& element : getChildren()){
				element->update(delta_in_ticks);
			}
		}

		virtual void layoutChildren(/*Direction*/){
			for(const auto& element : getChildren()){
				element->tryLayout();
			}
		}

		void tryDraw(const Rect& clipSpace) const override{
			if(!NoClipWhenDraw && !inboundOf(clipSpace)) return;

			drawMain();

			const auto space = property.getValidBound_absolute().intersectionWith(clipSpace);
			drawChildren(space);

			drawPost();
		}

		virtual void drawChildren(const Rect& clipSpace) const{
			for(const auto& element : getChildren()){
				element->tryDraw(clipSpace);
			}
		}

		void setScene(Scene* manager) override{
			Element::setScene(manager);
			for(const auto& element : getChildren()){
				element->setScene(manager);
			}
		}

		bool resize(const Geom::Vec2 size) override{
			if(Element::resize(size)){
				const auto newSize = getSize();

				for (auto& element : getChildren()){
					setChildrenFillParentSize(*element, newSize);
				}

				tryLayout();
				return true;
			}

			return false;
		}

		bool updateAbsSrc(const Geom::Vec2 parentAbsSrc) override{
			if(Element::updateAbsSrc(parentAbsSrc)){
				for(const auto& element : getChildren()){
					element->updateAbsSrc(absPos());
				}
				return true;
			}
			return false;
		}


		template <std::derived_from<Element> E>
		decltype(auto) emplaceChildren(){
			auto ptr = ElementUniquePtr{this, scene, new E};
			auto rst = ptr.get();
			addChildren(std::move(ptr));
			return *static_cast<E*>(rst);
		}

		template <InvocableElemInitFunc Fn>
			requires (std::is_default_constructible_v<typename ElemInitFuncTraits<Fn>::ElemType>)
		decltype(auto) emplaceInitChildren(Fn init){
			auto ptr = ElementUniquePtr{this, scene, init};
			auto rst = ptr.get();
			addChildren(std::move(ptr));
			return *static_cast<typename ElemInitFuncTraits<Fn>::ElemType*>(rst);
		}

	protected:
		/**
		 * @return true if all set by parent size
		 */
		static bool setChildrenFillParentSize(Element& item, Geom::Vec2 boundSize){
			const auto [fx, fy] = item.prop().fillParent;
			if(!fx && !fy) return false;

			const auto [vx, vy] = boundSize;
			const auto [ox, oy] = item.getSize();
			item.resize({
					fx ? vx : ox,
					fy ? vy : oy
				});

			return fx && fy;
		}
	// 	void modifyChildren(Element& element);
	};
}

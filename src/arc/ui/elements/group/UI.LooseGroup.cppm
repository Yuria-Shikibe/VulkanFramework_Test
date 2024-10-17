export module Core.UI.LooseGroup;

import Core.UI.ElementUniquePtr;
export import Core.UI.Group;

import std;

namespace Core::UI{
	CellBase EmptyCell;

	export
	struct BasicGroup : public Group {
	protected:
		std::vector<ElementUniquePtr> toRemove{};
		std::vector<ElementUniquePtr> children{};

	public:
		[[nodiscard]] explicit BasicGroup()
			: Group{}{
			interactivity = Interactivity::childrenOnly;
		}

		[[nodiscard]] explicit BasicGroup(std::string_view tyName)
			: Group{tyName}{
			interactivity = Interactivity::childrenOnly;
		}

		void postRemove(Element* element) override{
			if(const auto itr = find(element); itr != children.end()){
				toRemove.push_back(std::move(*itr));
				children.erase(itr);
			}
		}

		void instantRemove(Element* element) override{
			if(const auto itr = find(element); itr != children.end()){
				children.erase(itr);
			}
		}

		Element& addChildren(ElementUniquePtr&& element) override{
			setChildrenFillParentSize(*element, getSize());

			notifyLayoutChanged(SpreadDirection::upper);
			return *children.emplace_back(std::move(element));
		}

		[[nodiscard]] std::span<const ElementUniquePtr> getChildren() const noexcept override{
			return children;
		}

		[[nodiscard]] bool hasChildren() const noexcept override{
			return !children.empty();
		}

		void update(const float delta_in_ticks) override{
			toRemove.clear();

			Element::update(delta_in_ticks);

			updateChildren(delta_in_ticks);
		}

		void tryLayout() override{
			if(layoutState.isChanged()){
				layout();
			}else if(layoutState.isChildrenChanged()){//TODO accurate register change instead of for each?
				layoutChildren();
			}
		}

	protected:
		auto find(Element* element){
			return std::ranges::find(children, element, &ElementUniquePtr::get);
		}
	};

	export
	struct LooseGroup : BasicGroup{
		using BasicGroup::BasicGroup;

		void tryLayout() override{
			if(layoutState.isChanged()){
				layout();
				layoutChildren();
			}else if(layoutState.isChildrenChanged()){//TODO accurate register change instead of for each?
				layoutChildren();
			}
		}
	};
}

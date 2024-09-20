//
// Created by Matrix on 2024/9/19.
//

export module Core.UI.UniversalGroup;

export import Core.UI.Element;
export import Core.UI.CellBase;
import ext.concepts;
import std;

export namespace Core::UI{
	template <std::derived_from<CellBase> T>
	struct CellAdaptor{
		using CellType = T;
		Element* element{};
		T cell{};

		[[nodiscard]] constexpr CellAdaptor() noexcept = default;

		[[nodiscard]] constexpr CellAdaptor(Element* element, const T& cell) noexcept
			: element{element},
			  cell{cell}{}

	protected:
		constexpr void restrictSize() const noexcept{
			element->prop().clampedSize.setMaximumSize(cell.allocatedBound.getSize());
			//TODO move this to other place?
			element->layoutState.restrictedByParent = true;
		}
	};

	template <std::derived_from<CellBase> T, std::derived_from<CellAdaptor<T>> Adaptor = CellAdaptor<T>>
		requires requires(Element* e, const T& cell){
			Adaptor{e, cell};
		}
	struct UniversalGroup : public Group{
		T defaultCell{};

	protected:
		std::vector<std::unique_ptr<Element>> toRemove{};
		std::vector<std::unique_ptr<Element>> children{};
		std::vector<Adaptor> cells{};


	public:
		void postRemove(Element* element) override{
			if(const auto itr = find(element); itr != children.end()){
				cells.erase(cells.begin() + (itr - children.begin()));
				toRemove.push_back(std::move(*itr));
				children.erase(itr);
			}
		}

		void instantRemove(Element* element) override{
			if(const auto itr = find(element); itr != children.end()){
				cells.erase(cells.begin() + (itr - children.begin()));
				children.erase(itr);
			}
		}

		T& addChildren(std::unique_ptr<Element>&& element) override{
			modifyChildren(element.get());
			element->layoutState.acceptMask_context -= SpreadDirection::child;

			auto& cell = cells.emplace_back(element.get(), defaultCell);
			children.push_back(std::move(element));

			layoutState.setSelfChanged();

			return cell.cell;
		}

		[[nodiscard]] std::span<const std::unique_ptr<Element>> getChildren() const noexcept override{
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
			}else if(layoutState.isChildrenChanged()){
				layoutChildren();
			}
		}


		//TODO using explicit this
		template <std::derived_from<Element> E, typename... Args>
		T& emplace(Args&& ...args){
			std::unique_ptr<Element> ptr = std::make_unique<E>(std::forward<Args>(args)...);
			return addChildren(std::move(ptr));
		}

		template <typename E/*, std::derived_from<Group> This*/, typename Init>
			requires (std::is_default_constructible_v<E>)
		T& emplaceInit(Init&& init){
			std::unique_ptr<Element> ptr = std::make_unique<E>();
			init(*static_cast<E*>(ptr.get()));
			return addChildren(std::move(ptr));
		}

	protected:
		auto find(Element* element){
			return std::ranges::find(children, element, &std::unique_ptr<Element>::get);
		}
	};

}

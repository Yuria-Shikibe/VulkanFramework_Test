//
// Created by Matrix on 2024/9/19.
//

export module Core.UI.UniversalGroup;

export import Core.UI.Group;
export import Core.UI.Element;
export import Core.UI.CellBase;
export import Core.UI.LooseGroup;
import ext.concepts;
import std;

export namespace Core::UI{
	template <std::derived_from<CellBase> T>
		requires (std::is_copy_assignable_v<T> && std::is_default_constructible_v<T>)
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
		}

		constexpr void removeRestrict() const noexcept{
			element->prop().clampedSize.setMaximumSize(Geom::maxVec2<float>);

		}
	};

	template <std::derived_from<CellBase> CellTy, std::derived_from<CellAdaptor<CellTy>> Adaptor = CellAdaptor<CellTy>>
		requires requires(Element* e, const CellTy& cell){
			Adaptor{e, cell};
		}
	struct UniversalGroup : public BasicGroup{
		CellTy defaultCell{};

	protected:
		std::vector<Adaptor> cells{};

	public:
		[[nodiscard]] UniversalGroup() = default;

		[[nodiscard]] explicit UniversalGroup(const std::string_view tyName)
			: BasicGroup{tyName}{
		}

		[[nodiscard]] const std::vector<Adaptor>& getCells() const noexcept{
			return cells;
		}

		void postRemove(Element* element) override{
			if(const auto itr = find(element); itr != children.end()){
				cells.erase(cells.begin() + std::distance(children.begin(), itr));
				toRemove.push_back(std::move(*itr));
				children.erase(itr);
			}
		}

		void instantRemove(Element* element) override{
			if(const auto itr = find(element); itr != children.end()){
				cells.erase(cells.begin() + std::distance(children.begin(), itr));
				children.erase(itr);
			}
		}

		Element& addChildren(ElementUniquePtr&& element) override{
			element->layoutState.acceptMask_context -= SpreadDirection::child;
			return BasicGroup::addChildren(std::move(element));
		}

		template <std::derived_from<Element> E, typename ...Args>
			requires (std::constructible_from<E, Args...>)
		CellTy& emplace(Args&&... args){
			return add(ElementUniquePtr{this, scene, new E{std::forward<Args>(args) ...}});
		}

		template <InvocableElemInitFunc Fn>
			requires (std::is_default_constructible_v<typename ElemInitFuncTraits<Fn>::ElemType>)
		CellTy& emplaceInit(Fn init){
			return add(ElementUniquePtr{this, scene, init});
		}

	private:
		CellTy& add(ElementUniquePtr&& ptr){
			return cells.emplace_back(&addChildren(std::move(ptr)), defaultCell).cell;
		}
	};

}

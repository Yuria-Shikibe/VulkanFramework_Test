//
// Created by Matrix on 2024/9/14.
//

export module Core.UI.BedFace;

export import Core.UI.Element;
import Core.UI.UniversalGroup;
import Core.UI.Cell;

import std;

export namespace Core::UI{
	struct BedFaceCell : CellAdaptor<PassiveCell>{
		using CellAdaptor::CellAdaptor;
		void apply(const Geom::Vec2 absPos) const{
			element->layoutState.restrictedByParent = true;
			removeRestrict();
			cell.applyBoundToElement(element);
			restrictSize();
			cell.applyPosToElement(element, absPos);
		}
	};
	/**
	 * @brief A element bed for scaled layout
	 */
	struct BedFace : public UniversalGroup<PassiveCell, BedFaceCell>{


		void layout() override{
			const auto bound = Geom::OrthoRectFloat{Geom::FromExtent, property.boarder.bot_lft(), property.getValidSize()};
			for (auto&& value : cells){
				value.cell.allocate(bound);
				value.apply(absPos());
				value.element->layout();
			}

			Element::layout();
		}
	};
}

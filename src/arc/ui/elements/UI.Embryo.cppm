//
// Created by Matrix on 2024/9/19.
//

export module Core.UI.CellEmbryo;

export import Core.UI.UniversalGroup;
export import Core.UI.Cell;
import Core.UI.Util;

import std;

namespace Core::UI{
	struct EmbryoCell : CellAdaptor<PassiveCell>{
		using CellAdaptor::CellAdaptor;

		void apply(const Geom::Vec2 absPos) const{
			removeRestrict();
			cell.applyBoundToElement(element);
			restrictSize();
			cell.applyPosToElement(element, absPos);
		}

		bool endRow{};
	};


	export struct Embryo_Unsaturated : UniversalGroup<EmbryoCell::CellType, EmbryoCell>{
		Geom::USize2 grid{};

		Embryo_Unsaturated& endRow(){
			if(cells.empty())return *this;
			cells.back().endRow = true;
			return *this;
		}

		[[nodiscard]] Embryo_Unsaturated(){
			layoutState.ignoreChildren();
			interactivity = Interactivity::childrenOnly;
		}

		void layout() override{
			arrangeLayout();

			Element::layout();
		}

	private:
		void arrangeLayout(){
			grid = Util::countRowAndColumn(cells, &EmbryoCell::endRow);

			if(!grid.area())return;
			const Geom::Vec2 splittedSize = getValidSize() / grid.as<float>();

			Geom::USize2 currentPos{};

			for(auto& cell : cells){
				auto off =
					Util::flipY(splittedSize * currentPos.as<float>(), property.getValidHeight(), splittedSize.y)
					+ property.boarder.bot_lft();

				cell.cell.allocate({Geom::FromExtent, off, splittedSize});
				cell.apply(absPos());
				cell.element->layout();

				currentPos.x++;

				if(cell.endRow){
					currentPos.y++;
					currentPos.x = 0;
				}
			}
		}
	};

	export struct Embryo_Saturated : UniversalGroup<EmbryoCell::CellType, EmbryoCell>{
		std::vector<std::uint32_t> grid{};


		Embryo_Saturated& endRow(){
			if(cells.empty())return *this;
			cells.back().endRow = true;
			return *this;
		}

		[[nodiscard]] Embryo_Saturated(){
			layoutState.ignoreChildren();
			interactivity = Interactivity::childrenOnly;
		}

		void layout() override{
			arrangeLayout();

			Element::layout();
		}

	private:
		void arrangeLayout(){
			grid = Util::countRowAndColumn_toVector(cells, &EmbryoCell::endRow);

			if(grid.empty())return;
			const float height = property.getValidHeight() / grid.size();

			std::size_t currentY{};
			float lastX{};

			for(auto& cell : cells){
				const float width = property.getValidWidth() / grid[currentY];
				auto off =
					Geom::Vec2{lastX, Util::flipY(height * currentY, property.getValidHeight(), height)}
					+ property.boarder.bot_lft();

				cell.cell.allocatedBound =
					{Geom::FromExtent, off, {width, height}};
				cell.apply(absPos());
				cell.element->layout();

				lastX += width;;

				if(cell.endRow){
					currentY++;
					lastX = 0;
				}
			}
		}
	};
}

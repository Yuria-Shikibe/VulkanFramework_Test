//
// Created by Matrix on 2024/9/19.
//

export module Core.UI.Table;

export import Core.UI.UniversalGroup;
export import Core.UI.Cell;

import std;
import ext.algo;
import ext.views;

namespace Core::UI{
	struct TableCell : CellAdaptor<MasteringCell>{
		using CellAdaptor::CellAdaptor;

		void apply(const Geom::Vec2 absPos) const{
			cell.applyBoundToElement(element);
			restrictSize();
			cell.applyPosToElement(element, absPos);
		}

		bool endRow{};
	};


	struct RowLayoutInfo{
		StatedLength height{0, LengthDependency::none};
		std::vector<StatedLength> elemWidth{};

		[[nodiscard]] float getRowWidth() const noexcept{
			return ext::algo::accumulate(elemWidth, 0.0f, {}, &StatedLength::val);
		}
	};

	export
	struct Table : UniversalGroup<MasteringCell, TableCell>{
		std::vector<std::uint32_t> grid{};

		auto& endRow(){
			if(cells.empty())return *this;
			cells.back().endRow = true;
			return *this;
		}

		[[nodiscard]] Table(){
			layoutState.ignoreChildren();
			interactivity = Interactivity::childrenOnly;
		}

		void layout() override{
			arrangeLayout();

			Element::layout();
		}

	protected:
		void arrangeLayout(){
			grid = Util::countRowAndColumn_toVector(cells, &TableCell::endRow);
			if(grid.empty())return;

			auto size = property.getValidSize();

			const bool endState = std::exchange(cells.back().endRow, true);

			std::vector<RowLayoutInfo> infos(grid.size());

			auto zip = std::views::zip(infos, cells | ext::views::split_if(&TableCell::endRow));

			for (const auto& [row, rowData] : zip | std::views::enumerate){
				auto& [rowInfo, rowCells] = rowData;
				rowInfo.elemWidth.resize(rowCells.size());

				for (const auto & [column, cell] : rowCells | std::views::enumerate){
					Geom::Point2U curPos{static_cast<std::uint32_t>(column), static_cast<std::uint32_t>(row)};
					auto acquiredSize = cell.cell.size.crop();

					rowInfo.elemWidth[column] = acquiredSize.x;
					if(acquiredSize.y.dep <= rowInfo.height.dep){
						rowInfo.height.dep = acquiredSize.y.dep;
						rowInfo.height.val = std::max(rowInfo.height.val, acquiredSize.y.val);
					}
				}
			}

			cells.back().endRow = endState;
		}

	};
}

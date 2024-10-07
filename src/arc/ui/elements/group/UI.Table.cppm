//
// Created by Matrix on 2024/9/19.
//

export module Core.UI.Table;

export import Core.UI.UniversalGroup;
export import Core.UI.Cell;
export import Core.UI.TableLayout;

import std;
import ext.algo;
import ext.views;
import ext.meta_programming;

export namespace Core::UI{
	struct FixedTable : BasicTable{
		[[nodiscard]] FixedTable() : BasicTable{"FixedTable"}{
			layoutState.ignoreChildren();
		}

		void layout() override{
			arrangeLayout();

			Element::layout();
		}

	protected:
		void arrangeLayout(){
			grid = Util::countRowAndColumn_toVector(cells, &TableCellAdaptor::endRow);
			if(grid.empty())return;

			TableLayoutInfo layoutInfo{std::ranges::max(grid), grid.size()};
			layoutInfo.preRegisterAcquiredSize<false>(cells);

			//Get dependencies
			const auto dependencyCount = layoutInfo.getDependencyCount();
			const auto capturedSize_initial = layoutInfo.getCapturedSize();

			const auto validSize = property.getValidSize();
			const auto remainArea = validSize - capturedSize_initial;
			const auto slavedUnitSize = (remainArea / dependencyCount.as<float>()).infTo0();

			layoutInfo.alignWith(slavedUnitSize);

			//apply size & position
			const auto capturedSize_full = layoutInfo.arrange(cells);
			const auto alignOffset = Align::getOffsetOf(align, capturedSize_full, Rect{property.getValidSize()});

			for (auto& cell : cells){
				cell.cell.allocatedBound.src += alignOffset;
				cell.cell.allocatedBound.src.y += capturedSize_full.y;

				cell.cell.applyPosToElement(cell.element, absPos());
				cell.element->layout();
			}
		}
	};

	template <bool reguardItemSize>
	struct FlexTable : BasicTable{
		static constexpr std::string_view TypeNameStr{
			reguardItemSize ? "FlexTable<true>" : "FlexTable<false>"
		};

		Geom::Vec2 minimumExtendedSize{};

		[[nodiscard]] FlexTable() : BasicTable{TypeNameStr}{
			if constexpr (reguardItemSize)layoutState.ignoreChildren();
		}

		void layout() override{
			arrangeLayout();

			Element::layout();
		}

	protected:
		void arrangeLayout(){
			grid = Util::countRowAndColumn_toVector(cells, &TableCellAdaptor::endRow);
			if(grid.empty())return;

			TableLayoutInfo layoutInfo{std::ranges::max(grid), grid.size()};
			layoutInfo.preRegisterAcquiredSize<reguardItemSize>(cells);

			//Get dependencies
			const auto dependencyCount = layoutInfo.getDependencyCount();
			const auto capturedSize_initial = layoutInfo.getCapturedSize();

			auto validSize = property.getValidSize();

			bool resized = false;
			if(capturedSize_initial.x > validSize.x + minimumExtendedSize.x){
				resized = true;
				validSize.x = capturedSize_initial.x + minimumExtendedSize.x;
			}

			if(capturedSize_initial.y > validSize.y + minimumExtendedSize.y){
				resized = true;
				validSize.y = capturedSize_initial.y + minimumExtendedSize.y;
			}

			if(resized){
				this->resize_quiet(validSize);
			}

			const auto remainArea = validSize - capturedSize_initial;
			const auto slavedUnitSize = (remainArea / dependencyCount.as<float>()).infTo0();

			layoutInfo.alignWith(slavedUnitSize);

			//apply size & position
			const auto capturedSize_full = layoutInfo.arrange(cells);
			const auto alignOffset = Align::getOffsetOf(align, capturedSize_full, Rect{property.getValidSize()});

			for (auto& cell : cells){
				cell.cell.allocatedBound.src += alignOffset;
				cell.cell.allocatedBound.src.y += capturedSize_full.y;

				cell.cell.applyPosToElement(cell.element, absPos());
				cell.element->layout();
			}

			if(resized){
				notifyLayoutChanged(SpreadDirection::super);
			}
		}
	};

	using FlexTable_Dynamic = FlexTable<true>;
	using FlexTable_Static = FlexTable<false>;
}

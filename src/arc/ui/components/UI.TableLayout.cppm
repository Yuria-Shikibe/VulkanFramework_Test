//
// Created by Matrix on 2024/9/24.
//

export module Core.UI.TableLayout;

export import Core.UI.UniversalGroup;
export import Core.UI.Cell;
export import Core.UI.StatedLength;

import std;
import ext.views;
import ext.algo;

import Core.UI.Test;

export namespace Core::UI{
	struct TableCellAdaptor : CellAdaptor<TableCell>{
		using CellAdaptor::CellAdaptor;

		void apply(const Geom::Vec2 absPos) const{
			if(!cell.size.x.external() && !cell.size.y.external()){
				element->layoutState.restrictedByParent = true;
			}

			removeRestrict();
			cell.applyBoundToElement(element);
			restrictSize();
			cell.applyPosToElement(element, absPos);
		}

		bool endRow{};
	};

	class RowInfo{
	public:
		// Geom::Vec2 size{};
		std::vector<StatedSize> sizeData{};
	};

	class TableLayoutInfo{
		std::size_t maxColumn_{};
		std::vector<StatedLength> maxSizes{};

	public:
		using CellType = TableCellAdaptor;

		std::vector<RowInfo> rowInfos{};

		[[nodiscard]] TableLayoutInfo() = default;

		[[nodiscard]] TableLayoutInfo(const std::size_t maxColumn, const std::size_t maxRow)
			: maxColumn_{maxColumn}, rowInfos(maxRow){
			maxSizes.resize(maxColumn + maxRow);
		}

		[[nodiscard]] constexpr std::size_t maxRow() const noexcept{
			return maxSizes.size() - maxColumn_;
		}

		[[nodiscard]] constexpr std::size_t maxColumn() const noexcept{
			return maxColumn_;
		}

		[[nodiscard]] constexpr auto& atColumn(const std::size_t column) noexcept{
			return maxSizes[column];
		}

		[[nodiscard]] constexpr auto& atRow(const std::size_t row) noexcept{
			return maxSizes[maxColumn_ + row];
		}

		[[nodiscard]] constexpr auto& atColumn(const std::size_t column) const noexcept{
			return maxSizes[column];
		}

		[[nodiscard]] constexpr auto& atRow(const std::size_t row) const noexcept{
			return maxSizes[maxColumn_ + row];
		}

		[[nodiscard]] constexpr Geom::Vec2 sizeAt(const std::size_t column, const std::size_t row) const noexcept{
			return {atColumn(column).val, atRow(row).val};
		}

		[[nodiscard]] constexpr auto& data() const noexcept{
			return maxSizes;
		}

		constexpr std::pair<StatedLength&, StatedLength&> at(const std::size_t column, const std::size_t row) noexcept{
			return {(atColumn(column)), atRow(row)};
		}

		template <bool preAcquireSize = false>
		void preRegisterAcquiredSize(const std::vector<CellType>& cells){
			auto zip =
				std::views::zip(
					rowInfos | std::views::enumerate,
					cells | ext::views::part_if(&TableCellAdaptor::endRow)
				);
			//pre register acquire state

			for(const std::tuple<std::tuple<long long, RowInfo&>, std::ranges::subrange<std::vector<CellType>::const_iterator>>&
			    tuple : zip){
				const auto& [row, rowInfo, rowCells] = ext::flat_tuple<false>(tuple);
				rowInfo.sizeData.resize(rowCells.size());

				for(const auto& [column, cell] : rowCells | std::views::enumerate){
					auto acquiredSize = cell.cell.size;

					if constexpr (preAcquireSize){
						const bool xExternal = acquiredSize.x.external();
						const bool yExternal = acquiredSize.y.external();

						if(xExternal || yExternal){
							const auto usedSize = acquiredSize.cropMasterRatio();
							//set basic size
							// cell.element->resize_quiet(usedSize.getConcreteSize());

							auto result = cell.element->requestSpace(usedSize);

							result += cell.cell.getMarginSize();
							if(xExternal) acquiredSize.x.setConcreteSize(result.x);
							if(yExternal) acquiredSize.y.setConcreteSize(result.y);
						}
					}

					acquiredSize = acquiredSize.cropMasterRatio();

					rowInfo.sizeData[column] = acquiredSize;
					const auto [w, h] = acquiredSize.getConcreteSize();
					const auto [c, r] = at(column, row);
					c.val = std::max(c.val, w);
					c.promoteIndependence(acquiredSize.x.dep);

					r.val = std::max(r.val, h);
					r.promoteIndependence(acquiredSize.y.dep);
				}
			}
		}

		[[nodiscard]] Geom::USize2 getDependencyCount() const noexcept{
			Geom::USize2 dependencyCount{};
			for(const auto& [row, rowInfo] : rowInfos | std::views::enumerate){
				bool lineHeightDependent = false;
				std::uint32_t rowDependency{};
				for(const auto& [column, size] : rowInfo.sizeData | std::views::enumerate){
					lineHeightDependent |= !size.y.mastering();

					if(!size.x.mastering() && !atColumn(column).mastering()){
						++rowDependency;
					}
				}

				lineHeightDependent &= !atRow(row).mastering();

				dependencyCount.maxX(rowDependency);
				dependencyCount.y += static_cast<std::uint32_t>(lineHeightDependent);
			}

			return dependencyCount;
		}

		[[nodiscard]] constexpr Geom::Vec2 getCapturedSize() const noexcept{
			const Geom::Vec2 capturedSize{
					ext::algo::accumulate(maxSizes.begin(), maxSizes.begin() + maxColumn_, 0.f, {}, &StatedLength::val),
					ext::algo::accumulate(maxSizes.begin() + maxColumn_, maxSizes.end(), 0.f, {}, &StatedLength::val),
				};

			return capturedSize;
		}

		constexpr void alignWith(const Geom::Vec2 unitSize) noexcept{
			for(const auto& [row, info] : rowInfos | std::views::enumerate){
				auto& r = atRow(row);
				const float lineHeight = r.mastering() ? r.val : unitSize.y;

				for(const auto& [column, elementSize] : info.sizeData | std::views::enumerate){
					auto& c = atColumn(column);

					const Geom::Vec2 recommendedSize{c.mastering() ? c.val : unitSize.x, lineHeight};
					elementSize.trySetSlavedSize(recommendedSize);

					const Geom::Vec2 maxSize{
							elementSize.x.mastering() ? elementSize.x.val : recommendedSize.x,
							elementSize.y.mastering() ? elementSize.y.val : recommendedSize.y
						};

					c.val = std::max(c.val, maxSize.x);
					r.val = std::max(r.val, maxSize.y);
				}
			}
		}

		/**
		 * @brief arrange the cells to its initial position and apply its ratio
		 * @param cells cells to apply
		 * @return captured size
		 */
		[[nodiscard]] constexpr Geom::Vec2 arrange(std::vector<CellType>& cells) const noexcept{
			//apply size & position
			float maxX{};
			Geom::Vec2 offset{};

			auto zip =
				std::views::zip(
					rowInfos | std::views::enumerate,
					cells | ext::views::part_if(&TableCellAdaptor::endRow)
				)
				| std::views::transform(ext::tuple_flatter<false>{});

			for(const auto& [row, rowInfo, rowCells] : zip){
				float lineHeight{};

				for(const auto& [column, cell] : rowCells | std::views::enumerate){
					auto size = rowInfo.sizeData[column];

					const Geom::Vec2 curMaxSize = sizeAt(column, row);

					//TODO post(after ratio) apply the size?
					if(size.x.isFromRatio()) size.x.val = cell.cell.size.x.val;
					else if(size.y.isFromRatio()) size.y.val = cell.cell.size.y.val;

					size.applyRatio(curMaxSize);

					auto rawSize = size.getRawSize();
					if(!size.y.mastering()){
						rawSize.y = curMaxSize.y;
					}

					const auto off = Util::flipY(offset, 0, curMaxSize.y);

					Rect externalBound{Geom::FromExtent, off, curMaxSize};

					auto boundOffset = Align::getOffsetOf(cell.cell.insaturateAlign, rawSize, externalBound);

					cell.cell.allocate({Geom::FromExtent, boundOffset, rawSize});
					cell.cell.applyBoundToElement(cell.element);
					//TODO allocated bound align to the cell

					offset.x += curMaxSize.x;
					lineHeight = std::max(lineHeight, cell.cell.allocatedBound.getHeight());
				}

				maxX = std::max(maxX, offset.x);
				offset.x = 0;
				offset.y += lineHeight;
			}

			offset.x = maxX;
			return offset;
		}
	};

	struct BasicTable : UniversalGroup<TableCellAdaptor::CellType, TableCellAdaptor>{
		std::vector<std::uint32_t> grid{};
		Align::Pos align = Align::Pos::center;

		auto& endRow(){
			if(cells.empty())return *this;
			cells.back().endRow = true;
			return *this;
		}

		[[nodiscard]] BasicTable(const std::string_view ty) : UniversalGroup{ty}{
			interactivity = Interactivity::childrenOnly;
		}

		void layout() override = 0;

	protected:
		void drawMain() const override{
			UniversalGroup::drawMain();

			Test::drawCells(*this);
		}
	};

	static_assert(std::is_abstract_v<BasicTable>);
}

module;

export module Core.UI.Cell;

import Align;
import ext.concepts;
import std;

import Geom.Vector2D;
import Geom.Rect_Orthogonal;

import Core.UI.CellBase;

export namespace Core::UI {
	struct Element;
	using Rect = Geom::OrthoRectFloat;

	enum struct LengthDependency : std::uint8_t{
		master,
		masterRatio,
		slaving,
		slavingRatio,
		none,
	};

	struct StatedLength{
		float val{};
		LengthDependency dep{};

		[[nodiscard]] constexpr decltype(auto) getDependency() const noexcept{
			return std::to_underlying(dep);
		}

		[[nodiscard]] constexpr bool dependent() const noexcept{
			return dep != LengthDependency::master;
		}

		[[nodiscard]] constexpr bool mastering() const noexcept{
			return dep == LengthDependency::master;
		}

		[[nodiscard]] constexpr float crop() const noexcept{
			if(!dependent())return 0;
			return val;
		}
	};

	class ScaledSize{
	protected:
		Geom::Vec2 size{};

		static constexpr bool isScale(const float val) noexcept{
			return val < 0;
		}

		static constexpr bool isSize(const float val) noexcept{
			return val > 0;
		}

		/**
		 * @brief cell size is determined by parent
		 */
		static constexpr bool passive(const float val) noexcept{
			return val == 0;
		}

		/**
		 * @brief cell size is determined by item
		 */
		static bool adaptive(const float val) noexcept{
			return std::isnan(val);
		}

	public:
		[[nodiscard]] ScaledSize() = default;

		[[nodiscard]] explicit ScaledSize(const Geom::Vec2 size)
			: size{size}{}


		ScaledSize(const ScaledSize& other) = default;

		ScaledSize(ScaledSize&& other) noexcept = default;

		ScaledSize& operator=(const ScaledSize& other) = default;

		ScaledSize& operator=(ScaledSize&& other) noexcept = default;

		ScaledSize& operator=(const Geom::Vec2 v) noexcept{
			size = v;
			checkValid();
			return *this;
		}

		// [[nodiscard]] bool empty() const{
		// 	return size.isNaN() || (size.area() == 0);
		// }

		void checkValid() const{
			if((isScale(size.x) && isScale(size.y)) || size.isNaN()){
				throw new std::invalid_argument("invalid size");
			}
		}

		void set(const Geom::Vec2 size){
			this->size = size;
			checkValid();
		}

		void setWidth(const float w){
			this->size.x = w;
			checkValid();
		}

		void setHeight(const float h){
			this->size.y = h;
			checkValid();
		}

		[[nodiscard]] Geom::Vec2 get() const noexcept{
			auto rst = size;
			if(isScale(rst.x)){ // NOLINT(*-branch-clone)
				rst.x = std::abs(rst.x) * size.y;
			}else if(isScale(rst.y)){
				rst.y = std::abs(rst.y) * size.x;
			}
			return rst;
		}

		[[nodiscard]] constexpr bool isFetchX() const noexcept{
			return passive(size.x);
		}

		[[nodiscard]] constexpr bool isFetchY() const noexcept{
			return passive(size.y);
		}

		[[nodiscard]] bool isAdaptiveX() const noexcept{
			return adaptive(size.x);
		}

		[[nodiscard]] bool isAdaptiveY() const noexcept{
			return adaptive(size.y);
		}
	};

	struct StatedSize{
		StatedLength x{}, y{};

		constexpr void checkValidity() const noexcept(!DEBUG_CHECK){
#if DEBUG_CHECK
			if(x.dep == y.dep){
				if(x.dep == LengthDependency::masterRatio || x.dep == LengthDependency::slavingRatio){
					throw std::invalid_argument("invalid size: circular ratio dependency.");
				}
			}
#endif
		}



		constexpr void setConcreteSize(const Geom::Vec2 size) noexcept{
			x = {size.x, LengthDependency::master};
			y = {size.y, LengthDependency::master};
		}

		constexpr void setSize(const Geom::Vec2 size) noexcept{
			x.val = size.x;
			y.val = size.y;
		}

		constexpr void setState(const LengthDependency xState, const LengthDependency yState) noexcept{
			x.dep = xState;
			y.dep = yState;

			checkValidity();
		}

		constexpr std::pair<LengthDependency, LengthDependency> getState(){
			return {x.dep, y.dep};
		}

		[[nodiscard]] constexpr StatedSize crop() const noexcept{
			checkValidity();

			StatedSize crop{};
			crop.setState(x.dep, y.dep);

			if(x.dep == LengthDependency::masterRatio){
				if(y.mastering()){
					crop.y.val = y.val;
					crop.x.val = y.val * x.val;
					crop.x.dep = crop.y.dep = LengthDependency::master;
				}else{
					crop.x.dep = LengthDependency::slavingRatio;
				}
			}

			if(y.dep == LengthDependency::masterRatio){
				if(x.mastering()){
					crop.x.val = x.val;
					crop.y.val = x.val * y.val;
					crop.x.dep = crop.y.dep = LengthDependency::master;
				}else{
					crop.y.dep = LengthDependency::slavingRatio;
				}

			}

			return crop;
		}
	};

	struct PassiveCell : CellBase{
		static constexpr Rect FillScale{0, 0.0f, 1.0f, 1.0f};

	private:
		//external modifiable fields
		Align::Spacing margin{};
		Align::Pos scaleAlign{};
		Rect sizeScale{FillScale};

	public:
		constexpr PassiveCell& setScale(const Rect scale){
			sizeScale = scale;
			// Geom::Vec2 size = scale.getSize();
			// auto off = Align::getOffsetOf(align, size, FillScale);
			//
			// sizeScale = {Geom::fromExtent, off, size};

			return *this;
		}

		constexpr PassiveCell& setMargin(const Align::Spacing margin){
			this->margin = margin;
			return *this;
		}

		constexpr PassiveCell& setMargin(const float margin){
			this->margin.set(margin);
			return *this;
		}

		constexpr PassiveCell& setScaleAlign(const Align::Pos align){
			if(scaleAlign == align)return *this;
			scaleAlign = align;

			Geom::Vec2 size = sizeScale.getSize();
			auto off = Align::getOffsetOf(align, size, FillScale);

			sizeScale = {Geom::FromExtent, off, size};

			return *this;
		}
		constexpr PassiveCell& setAlign(const Align::Pos align){
			scaleAlign = align;

			return *this;
		}

		void applyBoundToElement(Element* element) const;
		void applyPosToElement(Element* element, Geom::Vec2 absSrc) const;
	};

	struct MasteringCell : PassiveCell{
		StatedSize size;

		Align::Spacing pad{};

		template <ext::number T = float>
		constexpr MasteringCell& setSize(const Geom::Vector2D<T> size){
			this->size.setConcreteSize(size.template as<float>());
			return *this;
		}

		constexpr MasteringCell& setSize(const float size){
			return setSize(Geom::Vec2{size, size});
		}

		constexpr MasteringCell& setSize(const float w, const float h){
			return setSize({w, h});
		}


	};

	// struct
	/*
	/**
	 * @code
	 *	  Inner   BOUND  Exter
	 *             |
	 *   Margin <--|--> Pad
	 *             |
	 * @endcode
	 #1#
	struct LayoutCell {
	protected:
		void changed(const ChangeSignal signal = ChangeSignal::notifyAll) const {
			if(item) {
				item->changed(signal);
			}
		}
		//Weak Reference Object
		Elem* item{nullptr};

		/**
		 * Nan for disable
		 * Negative for ratio
		 #1#
		Geom::Vec2 defSize{Geom::SNAN2};

		UI::Rect allocatedBound{};

		Align::Spacing margin{};

		Align::Spacing pad{};

		//scales are done after margin, so it calculates the size shrunk after margin
		UI::Rect scale{0, 0.0f, 1.0f, 1.0f};

		Align::Pos align = Align::Pos::bottom_left;

		/**
		 * \brief When true, the cell will expand parent group when the room isn't enough.
		 #1#
		bool modifyParentX{false};
		bool modifyParentY{false};

		bool modifyParentX_ifLarger{false};
		bool modifyParentY_ifLarger{false};

		bool scaleRelativeToParentX{true};
		bool scaleRelativeToParentY{true};

		friend Table;
	public:
		[[nodiscard]] constexpr LayoutCell() noexcept = default;

		[[nodiscard]] constexpr explicit LayoutCell(Elem* item) noexcept
			: item{item}{}

		[[nodiscard]] constexpr Elem* getItem() const noexcept{ return item; }

		[[nodiscard]] bool isEndRow() const {
			return item->isEndingRow();
		}

		LayoutCell& endLine(){
			if(item)item->setEndRow(true);

			return *this;
		}

		LayoutCell& setSize(const float w, const float h, const bool usesExpand = false){
			setWidth(w, usesExpand);
			setHeight(h, usesExpand);

			return *this;
		}

		LayoutCell& setSize(const int w, const int h){
			setWidth(w, false);
			setHeight(h, false);

			return *this;
		}

		// LayoutCell& setSize(const float s, const int) = delete;

		LayoutCell& setSize(const float s, const bool usesExpand = false){
			return setSize(s, s, usesExpand);
		}

		LayoutCell& setWidth(const float w, const bool usesExpand = false){
			defSize.x = w;
			usesExpand ? expandX(true) : wrapX();

			return *this;
		}

		LayoutCell& setHeight(const float h, const bool usesExpand = false){
			defSize.y = h;
			usesExpand ? expandY(true) : wrapY();

			return *this;
		}

		LayoutCell& wrapX() {
			changed();
			scaleRelativeToParentX = false;
			modifyParentX = true;
			modifyParentX_ifLarger = false;
			return *this;
		}

		LayoutCell& setSlave(){
			scaleRelativeToParentX = scaleRelativeToParentY = false;
			modifyParentX_ifLarger = modifyParentY_ifLarger = false;
			modifyParentX = modifyParentY = false;

			return *this;
		}

		LayoutCell& wrapY() {
			changed();
			scaleRelativeToParentY = false;
			modifyParentY = true;
			modifyParentY_ifLarger = false;

			return *this;
		}

		LayoutCell& wrap() {
			wrapX();
			wrapY();
			return *this;
		}

		LayoutCell& fillParentX() {
			changed();
			scaleRelativeToParentX = true;
			return *this;
		}

		LayoutCell& fillParentY() {
			changed();
			scaleRelativeToParentY = true;
			return *this;
		}

		LayoutCell& fillParent() {
			changed();
			scaleRelativeToParentX = true;
			scaleRelativeToParentY = true;
			return *this;
		}

		LayoutCell& setMargin(const float left, const float right, const float top, const float bottom) {
			margin = {left, right, bottom, top};
			changed();

			return *this;
		}

		LayoutCell& setMargin(const float val) {
			margin.set(val);
			changed();

			return *this;
		}

		LayoutCell& setMargin(const Align::Spacing margin) {
			if(this->margin != margin){
				this->margin = margin;
				changed();
			}

			return *this;
		}

		LayoutCell& setPad(const Align::Spacing pad) {
			if(this->pad != pad){
				this->pad = pad;
				changed();
			}

			return *this;
		}

		LayoutCell& setXRatio(const float y2xRatio = 1.f){
			defSize.x = -y2xRatio;
			wrapX();

			if(defSize.x < 0 && defSize.y < 0){
				throw ext::IllegalArguments{"Layout Cannot Have To Loop-Referenced Ratio!"};
			}

			return *this;
		}

		LayoutCell& setYRatio(const float x2yRatio = 1.f){
			defSize.y = -x2yRatio;
			wrapY();

			if(defSize.x < 0 && defSize.y < 0){
				throw ext::IllegalArguments{"Layout Cannot Have To Loop-Referenced Ratio!"};
			}

			return *this;
		}

		LayoutCell& expandX(const bool val = true) {
			val ? wrapX() : fillParentX();
			modifyParentX_ifLarger = modifyParentX = val;
			return *this;
		}

		LayoutCell& expandY(const bool val = true) {
			val ? wrapY() : fillParentY();
			modifyParentY_ifLarger = modifyParentY = val;
			return *this;
		}

		LayoutCell& expand(const bool valX = true, const bool valY = true) {
			expandX(valX);
			expandY(valY);
			return *this;
		}

		LayoutCell& applyLayout(const LayoutCell& other) {
			if(this == &other) return *this;
			changed();
			const auto cur = item;
			*this = other;
			item = cur;
			return *this;
		}

		LayoutCell& setAlign(const Align::Pos align = Align::Pos::center) {
			if(align == this->align)return *this;
			changed();
			this->align = align;

			return *this;
		}

		LayoutCell& setSrcScale(const float xScl, const float yScl, const bool move = true) {
			changed();

			if(move) {
				scale.setSrc(xScl, yScl);
			}else {
				scale.setVert({xScl, yScl}, scale.getEnd());
			}

			return *this;
		}

		LayoutCell& setEndScale(const float xScl, const float yScl) {
			changed();

			scale.setVert(scale.getSrc(), {xScl, yScl});

			return *this;
		}

		LayoutCell& setSizeScale(const float xScl, const float yScl, const Align::Pos align = Align::Pos::center, const bool needClearRelaMove = true) {
			changed();

			Align::Spacing tempScale{scale.getSrcX(), scale.getEndX(), scale.getSrcY(), scale.getEndY()};

			if(align & Align::Pos::top) {
				tempScale.bottom = tempScale.top - yScl;
			}else if(align & Align::Pos::bottom){
				tempScale.top = tempScale.bottom + yScl;
			}else { //centerY
				tempScale.top = 0.5f + yScl * 0.5f;
				tempScale.bottom = 0.5f - yScl * 0.5f;
			}

			if(align & Align::Pos::right) {
				tempScale.left = tempScale.right - xScl;
			}else if(align & Align::Pos::left){
				tempScale.right = tempScale.left + xScl;
			}else { //centerX
				tempScale.right = 0.5f + xScl * 0.5f;
				tempScale.left = 0.5f - xScl * 0.5f;
			}

			scale.setVert(tempScale.bot_lft(), tempScale.top_rit());

			changed();
			if(needClearRelaMove)this->clearRelativeMove();

			return *this;
		}

		LayoutCell& clearRelativeMove() {
			changed();
			scale.setSrc(0, 0);

			return *this;
		}

		[[nodiscard]] float getCellWidth() const {return allocatedBound.getWidth();}
		[[nodiscard]] float getCellHeight() const {return allocatedBound.getHeight();}
		[[nodiscard]] float getScaledCellHeight() const {return getVertScale() * getCellHeight();}
		[[nodiscard]] float getScaledCellWidth() const {return getHoriScale() * getCellWidth();}


		[[nodiscard]] float getDefWidth() const{
			return hasDefWidth() ? defSize.x : item ? item->getWidth() : 0;
		}

		[[nodiscard]] float getDefHeight() const{
			return hasDefHeight() ? defSize.y : item ? item->getHeight() : 0;
		}

		[[nodiscard]] float getExportWidth() const{
			float width = 0;
			if(hasDefWidth())width = defSize.x;
			else if(item)width = item->getWidth();

			if(hasRatioFromHeight())width = getRatioedWidth();
			return width;
		}

		[[nodiscard]] float getExportHeight() const{
			float height = 0;
			if(hasDefHeight())height = defSize.y;
			else if(item)height = item->getHeight();

			if(hasRatioFromWidth())height = getRatioedHeight();
			return height;
		}

		void applyRatio(){
			if(hasRatioFromHeight()){
				allocatedBound.setWidth(allocatedBound.getHeight() * getRatio_H2W());
			}

			if(hasRatioFromWidth()){
				allocatedBound.setHeight(allocatedBound.getWidth() * getRatio_W2H());
			}
		}

		[[nodiscard]] bool hasDefWidth() const{
			return !std::isnan(defSize.x) && defSize.x > 0;
		}

		[[nodiscard]] bool hasDefHeight() const{
			return !std::isnan(defSize.y) && defSize.y > 0;
		}

		[[nodiscard]] bool hasRatioFromHeight() const{
			return !std::isnan(defSize.x) && (defSize.area() <= 0 || std::isnan(defSize.y)) && defSize.x < 0;
		}

		[[nodiscard]] bool hasRatioFromWidth() const{
			return !std::isnan(defSize.y) && (defSize.area() <= 0 || std::isnan(defSize.x)) && defSize.y < 0;
		}

		[[nodiscard]] bool isSlaveX() const noexcept{
			return !(scaleRelativeToParentX || modifyParentX || modifyParentX_ifLarger);
		}

		[[nodiscard]] bool isSlaveY() const noexcept{
			return !(scaleRelativeToParentY || modifyParentY || modifyParentY_ifLarger);
		}

		[[nodiscard]] float getRatioedWidth() const{
			return hasRatioFromHeight() ? 0 : getExportHeight() * getRatio_H2W();
		}

		[[nodiscard]] float getRatioedHeight() const{
			return hasRatioFromWidth() ? 0 : getExportWidth() * getRatio_W2H();
		}

		[[nodiscard]] float getRatio_H2W() const{
			return -defSize.x;
		}

		[[nodiscard]] float getRatio_W2H() const{
			return -defSize.y;
		}

		[[nodiscard]] float getHoriScale() const {return scale.getWidth();}
		[[nodiscard]] float getVertScale() const {return scale.getHeight();}

		[[nodiscard]] float getMarginHori() const {return margin.getWidth();}
		[[nodiscard]] float getMarginVert() const {return margin.getHeight();}

		[[nodiscard]] float getPadHori() const {return pad.getWidth();}
		[[nodiscard]] float getPadVert() const {return pad.getHeight();}

		/**
		 * @return True if the cell size has changed
		 #1#
		bool applySizeToItem();

		void applyAlignToItem(const Rect bound) const;

		//Invoke this after all cell bound has been arranged.
		void applyPosToItem(Elem* parent) const;

		[[nodiscard]] bool isIgnoreLayout() const {
			return item->isIgnoreLayout();
		}

		template <Concepts::Derived<Elem> T>
		[[nodiscard]] T& as() {//TODO static cast maybe??
			return static_cast<T&>(*item);
		}
	};*/
}

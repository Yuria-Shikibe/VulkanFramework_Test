export module Core.UI.Element;

export import Geom.Rect_Orthogonal;
export import Core.UI.ClampedSize;

export import Align;
export import Core.UI.ElementUniquePtr;
export import Core.UI.ToolTipInterface;

export import Core.UI.Event;
export import Core.UI.Flags;
export import Core.UI.Util;
export import Core.UI.CellBase;
export import Core.UI.Action;

export import Core.Ctrl.Constants;

import Core.UI.StatedLength;

import Graphic.Color;

import ext.meta_programming;
import ext.handle_wrapper;
import ext.owner;
import std;

// export namespace Core{
// 	class Bundle;
// }
//
//
export namespace Graphic{
	struct RendererUI;
	struct Batch_Exclusive;
	// struct RendererUI;
}


namespace Core::UI{
	export constexpr bool NoClipWhenDraw = true;

	export using Rect = Geom::Rect_Orthogonal<float>;
	export struct ElementDrawer{
		virtual ~ElementDrawer() = default;

		virtual void draw(const Element& element) const = 0;
	};

	struct DefElementDrawer final : public ElementDrawer{
		void draw(const Element& element) const override;
	};

	export constexpr DefElementDrawer DefaultDrawer;

	export struct ElemGraphicData{
		Graphic::Color baseColor{};
		float inherentOpacity{1.f};
		float contextOpacity{1.f};


		[[nodiscard]] constexpr float getOpacity() const noexcept{
			return inherentOpacity * contextOpacity;
		}

		[[nodiscard]] constexpr Graphic::Color getScaledColor() const noexcept{
			return baseColor.copy().mulA(getOpacity());
		}

		// mutable Graphic::Color tmpColor{};
		//TODO drawer

		const ElementDrawer* drawer{&DefaultDrawer};
	};

	export
	template <typename T>
	struct ElemDynamicChecker{
		std::function<bool(T&)> visibilityChecker{nullptr};
		std::function<bool(T&)> disableChecker{nullptr};
		std::function<bool(T&)> activatedChecker{nullptr};

		std::function<void(T&)> appendUpdator{nullptr};
	};

	export
	struct CursorState{
		/**
		 * @brief in tick
		 */
		float focusedTime{};
		float stagnateTime{};

		bool inbound{};
		bool focused{};
		bool pressed{};

		void update(const float delta_in_ticks){
			if(focused){
				focusedTime += delta_in_ticks;
				stagnateTime += delta_in_ticks;
			}
		}

		void registerFocusEvent(ext::event_manager& event_manager){
			event_manager.on<Event::BeginFocus>([this](auto){
				focused = true;
			});

			event_manager.on<Event::EndFocus>([this](auto){
				focused = false;
				stagnateTime = focusedTime = 0.f;
			});

			event_manager.on<Event::Moved>([this](auto){
				stagnateTime = 0.f;
			});
		}

		void registerDefEvent(ext::event_manager& event_manager){
			event_manager.on<Event::EndFocus>([this](auto){
				pressed = false;
			});

			event_manager.on<Event::Inbound>([this](auto){
				inbound = true;
			});

			event_manager.on<Event::Exbound>([this](auto){
				inbound = false;
			});

			event_manager.on<Event::Click>([this](const Event::Click& click){
				switch(click.code.action()){
					case Ctrl::Act::Press: pressed = true; break;
					default: pressed = false;
				}
			});
		}
	};

	export struct ElemProperty{
		std::string name{};


		//TODO uses string view?
		std::string elementTypeName{};

		//position fields
		Geom::Vec2 relativeSrc{};
		Geom::Vec2 absoluteSrc{};

		Geom::Vector2D<bool> fillParent{};
		ClampedSize clampedSize{};
		Align::Spacing boarder{};

		float globalScale{};
		float localScale{};


		//state
		bool activated{}; //TODO as graphic property?
		bool visible{true};
		bool sleep{};
		bool maintainFocusUntilMouseDrop{};


		//input events
		ext::event_manager events{
			{
				ext::index_of<Event::Click>(),
				ext::index_of<Event::Drag>(),
				ext::index_of<Event::Exbound>(),
				ext::index_of<Event::Inbound>(),
				ext::index_of<Event::BeginFocus>(),
				ext::index_of<Event::EndFocus>(),
				ext::index_of<Event::Scroll>(),
				ext::index_of<Event::Moved>(),
			}
		};

		ElemGraphicData graphicData{};

		[[nodiscard]] ElemProperty() = default;

		[[nodiscard]] explicit ElemProperty(const std::string_view elementTypeName)
			: elementTypeName{elementTypeName}{}

		constexpr void setAbsoluteSrc(const Geom::Vec2 parentOriginalPoint) noexcept{
			absoluteSrc = parentOriginalPoint + relativeSrc;
		}

		[[nodiscard]] constexpr Rect getBound_relative() const noexcept{
			return Rect{Geom::FromExtent, relativeSrc, clampedSize.getSize()};
		}

		[[nodiscard]] constexpr Rect getBound_absolute() const noexcept{
			return Rect{Geom::FromExtent, absoluteSrc, clampedSize.getSize()};
		}

		[[nodiscard]] constexpr Rect getValidBound_relative() const noexcept{
			return Rect{Geom::FromExtent, relativeSrc + boarder.bot_lft(), getValidSize()};
		}

		[[nodiscard]] constexpr Rect getValidBound_absolute() const noexcept{
			return Rect{Geom::FromExtent, absoluteSrc + boarder.bot_lft(), getValidSize()};
		}

		[[nodiscard]] constexpr bool contains(const Rect& clipRegion, const Geom::Vec2 pos) const noexcept{
			return clipRegion.containsPos_edgeExclusive(pos) && getValidBound_absolute().containsPos_edgeExclusive(pos);
		}

		[[nodiscard]] constexpr Geom::Vec2 evaluateValidSize(Geom::Vec2 rawSize) const noexcept{
			return rawSize.clampXY(clampedSize.getMinimumSize(), clampedSize.getMaximumSize()).sub(boarder.getSize()).max(Geom::zeroVec2<float>);
		}

		[[nodiscard]] constexpr float getValidWidth() const noexcept{
			return clampedSize.getWidth() - boarder.getWidth();
		}

		[[nodiscard]] constexpr float getValidHeight() const noexcept{
			return clampedSize.getHeight() - boarder.getHeight();
		}

		[[nodiscard]] constexpr Geom::Vec2 getValidSize() const noexcept{
			return clampedSize.getSize() - boarder.getSize();
		}

	};

	export struct Element : StatedToolTipOwner<struct Element>{
	protected:
		ElemProperty property{};
		CursorState cursorState{};

		Group* parent{};
		Scene* scene{};

		std::queue<std::unique_ptr<Action<Element>>> actions{};

	public:
		//Layout Spec
		LayoutState layoutState{};
		Interactivity interactivity{Interactivity::enabled};

		[[nodiscard]] Element(){
			cursorState.registerFocusEvent(events());
		}

		[[nodiscard]] explicit Element(const std::string_view tyName)
			: property{tyName}{
			cursorState.registerFocusEvent(events());
		}

		virtual ~Element(){
			clearExternalReferences();
		}

		[[nodiscard]] Graphic::Batch_Exclusive& getBatch() const noexcept;

		[[nodiscard]] Graphic::RendererUI& getRenderer() const noexcept;

		[[nodiscard]] const CursorState& getCursorState() const noexcept{
			return cursorState;
		}

		void resize_quiet(const Geom::Vec2 size){
			auto last = std::exchange(layoutState.acceptMask_inherent, SpreadDirection::none);
			resize(size);
			layoutState.acceptMask_inherent = last;
		}

		[[nodiscard]] constexpr Geom::Vec2 getSize() const noexcept{
			return property.clampedSize.getSize();
		}

		[[nodiscard]] constexpr Geom::Vec2 getValidSize() const noexcept{
			return property.getValidSize();
		}

		[[nodiscard]] constexpr bool maintainFocusByMouse() const noexcept{
			return property.maintainFocusUntilMouseDrop;
		}

		[[nodiscard]] constexpr bool isActivated() const noexcept{ return property.activated; }

		[[nodiscard]] constexpr bool isVisible() const noexcept{ return property.visible; }

		[[nodiscard]] constexpr bool isSleep() const noexcept{ return property.sleep; }

		[[nodiscard]] Group* getParent() const noexcept{
			return parent;
		}

		void updateOpacity(float val);

		void setParent(Group* p) noexcept{
			parent = p;
		}

		[[nodiscard]] bool isRootElement() const noexcept{
			return parent == nullptr;
		}

		//TODO should manager be null?
		[[nodiscard]] Scene* getScene() const noexcept{
			return scene;
		}

		[[nodiscard]] ElemProperty& prop() noexcept{
			return property;
		}

		[[nodiscard]] const ElemProperty& prop() const noexcept{
			return property;
		}

		[[nodiscard]] const auto& graphicProp() const noexcept{
			return property.graphicData;
		}

		ext::event_manager& events() noexcept{
			return property.events;
		}

		[[nodiscard]] constexpr Geom::Vec2 absPos() const noexcept{
			return property.absoluteSrc;
		}


		[[nodiscard]] constexpr Geom::Vec2 contentSrcPos() const noexcept{
			return property.absoluteSrc + property.boarder.bot_lft();
		}

		void removeSelfFromParent();
		void removeSelfFromParent_instantly();

		void registerAsyncTask();

		void notifyRemove();

		void clearExternalReferences() noexcept;

		[[nodiscard]] bool isFocusedScroll() const noexcept;

		[[nodiscard]] bool isFocused() const noexcept;

		[[nodiscard]] bool isInbounded() const noexcept;

		void setFocusedScroll(bool focus) noexcept;
		//
		// [[nodiscard]] constexpr bool isQuietInteractable() const noexcept{
		// 	return (interactivity != Interactivity::enabled && hasTooltip()) && isVisible();
		// }

		[[nodiscard]] constexpr bool isInteractable() const noexcept{
			return (interactivity == Interactivity::enabled || hasTooltip()) && isVisible();
		}

		[[nodiscard]] constexpr bool touchDisabled() const noexcept{
			return interactivity == Interactivity::disabled;
		}

		[[nodiscard]] bool containsPos(Geom::Vec2 absPos) const noexcept;

		[[nodiscard]] bool containsPos_self(Geom::Vec2 absPos, float margin = 0.f) const noexcept;

		template<ElemInitFunc InitFunc>
		void setTooltipState(const ToolTipProperty& toolTipProperty, InitFunc&& initFunc) noexcept{
			StatedToolTipOwner::setTooltipState(toolTipProperty, std::forward<InitFunc>(initFunc));
			
			events().on<Event::Moved>([this](const auto&){
				dropToolTipIfMoved();
			});
		}

		void buildTooltip() noexcept;

		template <std::derived_from<Action<Element>> ActionType, typename ...T>
		void pushAction(T&&... args){
			actions.push(std::make_unique<ActionType>(std::forward<T>(args)...));
		}

		template <typename ...ActionType>
			requires (std::derived_from<std::decay_t<ActionType>, Action<Element>> && ...)
		void pushAction(ActionType&&... args){
			actions.push(std::make_unique<std::decay_t<ActionType>>(std::forward<ActionType>(args)) ...);
		}

		// ------------------------------------------------------------------------------------------------------------------
		// interface region
		// ------------------------------------------------------------------------------------------------------------------

		[[nodiscard]] virtual bool containsPos_parent(Geom::Vec2 cursorPos) const;

		virtual void notifyLayoutChanged(SpreadDirection toDirection);

		virtual void setScene(Scene* s){
			this->scene = s;
		}


		virtual void update(float delta_in_ticks);

		virtual void tryLayout(){
			if(layoutState.isChanged()){
				layout();
			}
		}

		virtual void layout(){
			layoutState.checkChanged();
		}

		[[nodiscard]] virtual Geom::Vec2 requestSpace(const StatedSize sz){
			return getSize();
		}

		[[nodiscard]] virtual bool hasChildren() const noexcept{
			return !getChildren().empty();
		}

		[[nodiscard]] virtual std::span<const ElementUniquePtr> getChildren() const noexcept{
			return {};
		}

		virtual void tryDraw(const Rect& clipSpace) const{
			if(NoClipWhenDraw || inboundOf(clipSpace)){
				drawMain();
			}
		}

		virtual void drawMain() const;

		virtual void drawPost() const{}

		virtual void inputKey(const int key, const int action, const int mode){}

		virtual bool resize(const Geom::Vec2 size /*, Direction Mask*/){
			if(property.clampedSize.setSize(size)){
				notifyLayoutChanged(SpreadDirection::all);
				return true;
			}
			return false;
		}

		virtual bool updateAbsSrc(const Geom::Vec2 parentAbsSrc){
			if(Util::tryModify(property.absoluteSrc, parentAbsSrc + property.relativeSrc)){
				notifyLayoutChanged(SpreadDirection::local);
				return true;
			}
			return false;
		}

		virtual void getFuture(){

		}

		[[nodiscard]] TooltipAlignPos getAlignPolicy() const override{
			switch(tooltipProp.followTarget){
				case TooltipFollow::depends:{
					return {Align::getVert(tooltipProp.followTargetAlign, property.getBound_absolute()), tooltipProp.tooltipSrcAlign};
				}

				default: return {tooltipProp.followTarget, tooltipProp.tooltipSrcAlign};
			}
		}

		[[nodiscard]] bool tooltipShouldDrop(Geom::Vec2 cursorPos) const override{
			return !containsPos(cursorPos);
		}

		[[nodiscard]] bool tooltipShouldBuild(Geom::Vec2 cursorPos) const override{
			return true
				&& hasTooltip()
				&& tooltipProp.autoBuild()
				&& (tooltipProp.useStagnateTime ? cursorState.stagnateTime : cursorState.focusedTime)
				> tooltipProp.minHoverTime;
		}

		void notifyDrop() override{
			cursorState.focusedTime = cursorState.stagnateTime = -15.f;
		}

	protected:
		[[nodiscard]] bool inboundOf(const Rect& clipSpace_abs) const noexcept{
			return clipSpace_abs.overlap_Exclusive(property.getValidBound_absolute());
		}

		void dropToolTipIfMoved() const;

	public:
		std::vector<Element*> dfsFindDeepestElement(Geom::Vec2 cursorPos);
	};

	void iterateAll_DFSImpl(Geom::Vec2 cursorPos, std::vector<struct Element*>& selected, struct Element* current);
}

module : private;

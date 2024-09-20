export module Core.UI.Element;

export import Geom.Rect_Orthogonal;
export import Core.UI.ClampedSize;

export import Align;

export import Core.UI.Event;
export import Core.UI.Flags;
export import Core.UI.Util;
export import Core.UI.CellBase;


export import Core.Ctrl.Constants;

import Graphic.Color;

import ext.meta_programming;
import ext.handle_wrapper;
import ext.owner;
import std;

export namespace Core{
	class Bundle;
}


export namespace Graphic{
	struct RendererUI;
}


namespace Core::UI{
	export using Rect = Geom::Rect_Orthogonal<float>;
	export struct Element;
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
		mutable Graphic::Color tmpColor{};
		float baseOpacity{};
		float opacity{};
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
		bool inbound{};
		bool focused{};
		bool pressed{};

		void registerDefEvent(ext::event_manager& event_manager){
			event_manager.on<Event::BeginFocus>([this](auto){
				focused = true;
			});

			event_manager.on<Event::EndFocus>([this](auto){
				pressed = focused = false;
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

	export struct Group;
	export struct Scene;

	export struct ElemProperty{
		std::string name{};

		std::string elementTypeName{};

		//position fields
		Geom::Vec2 relativeSrc{};
		Geom::Vec2 absoluteSrc{};

		Geom::Vector2D<bool> fillParent{};
		ClampedSize clampedSize{};

		float globalScale{};
		float localScale{};

		Align::Spacing boarder{};

		//state
		bool activated{};
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
				ext::index_of<Event::Scroll>()
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

	export struct Element{
	protected:
		ElemProperty property{};
		CursorState cursorState{};

		Group* parent{};
		Scene* scene{};
	public:
		//Layout Spec
		LayoutState layoutState{};
		Interactivity interactivity{Interactivity::enabled};

		[[nodiscard]] Element(){
			cursorState.registerDefEvent(events());
		}

		[[nodiscard]] explicit Element(const std::string_view tyName)
			: property{tyName}{
			cursorState.registerDefEvent(events());
		}

		virtual ~Element(){
			clearExternalReferences();
		}

		[[nodiscard]] const CursorState& getCursorState() const noexcept{
			return cursorState;
		}

		void resize_unchecked(const Geom::Vec2 size){
			property.clampedSize.setSize_unchecked(size);
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

		ext::event_manager& events() noexcept{
			return property.events;
		}

		[[nodiscard]] Geom::Vec2 absPos() const noexcept{
			return property.absoluteSrc;
		}

		void notifyRemove();

		void clearExternalReferences() noexcept;

		[[nodiscard]] bool isFocusedScroll() const noexcept;

		void setFocusedScroll(bool focus) noexcept;

		[[nodiscard]] constexpr bool isQuietInteractable() const noexcept{
			return (interactivity != Interactivity::enabled /*&& static_cast<bool>(tooltipbuilder)*/) && isVisible();
		}

		[[nodiscard]] constexpr bool isInteractable() const noexcept{
			return (interactivity == Interactivity::enabled/* || static_cast<bool>(tooltipbuilder)*/) && isVisible();
		}

		[[nodiscard]] constexpr bool touchDisabled() const noexcept{
			return interactivity == Interactivity::disabled;
		}

		[[nodiscard]] bool containsPos(Geom::Vec2 absPos) const noexcept;

		//interface region

		virtual bool containsPos_parent(Geom::Vec2 clipSpace){
			return true;
		}

		virtual void notifyLayoutChanged(SpreadDirection toDirection);

		virtual void setScene(Scene* s){
			this->scene = s;
		}


		virtual void update(float delta_in_ticks){

		}

		virtual void tryLayout(){
			if(layoutState.isChanged()){
				layout();
			}
		}

		virtual void layout(){
			layoutState.checkChanged();
		}

		[[nodiscard]] virtual Geom::Vec2 requestSpace() const noexcept{
			return getSize();
		}

		[[nodiscard]] virtual bool hasChildren() const noexcept{
			return !getChildren().empty();
		}

		[[nodiscard]] virtual std::span<const std::unique_ptr<Element>> getChildren() const noexcept{
			return {};
		}

		virtual void tryDraw(const Rect& clipSpace) const{
			if(inboundOf(clipSpace)){
				drawMain();
			}
		}

		virtual void drawMain() const{
			property.graphicData.drawer->draw(*this);
		}

		virtual void drawPost() const{

		}

		virtual void inputKey(const int key, const int action, const int mode){

		}

		virtual bool resize(const Geom::Vec2 size /*, Direction Mask*/){
			if(property.clampedSize.setSize(size)){
				notifyLayoutChanged(SpreadDirection::all);
				return true;
			}
			return false;
		}

		virtual bool updateAbsSrc(const Geom::Vec2 parentAbsSrc){
			if(Util::tryModify(property.absoluteSrc, parentAbsSrc + property.relativeSrc)){
				notifyLayoutChanged(SpreadDirection::lower);
				return true;
			}
			return false;
		}

	protected:
		[[nodiscard]] bool inboundOf(const Rect& clipSpace_abs) const noexcept{
			return clipSpace_abs.overlap_Exclusive(property.getValidBound_absolute());
		}
	};

	export struct Group : public Element{
		using Element::Element;

		virtual void postRemove(Element* element) = 0;
		virtual void instantRemove(Element* element) = 0;

		virtual CellBase& addChildren(std::unique_ptr<Element>&& element) = 0;

		//TODO using explicit this


		virtual void updateChildren(const float delta_in_ticks){
			for (const auto & element : getChildren()){
				element->update(delta_in_ticks);
			}
		}

		virtual void layoutChildren(/*Direction*/){
			for (const auto& element : getChildren()){
				element->tryLayout();
			}
		}

		void tryDraw(const Rect& clipSpace) const override{
			if(!inboundOf(clipSpace))return;

			drawMain();

			const auto space = property.getValidBound_absolute().getOverlap(clipSpace);
			drawChildren(space);

			drawPost();
		}

		virtual void drawChildren(const Rect& clipSpace) const{
			for (const auto & element : getChildren()){
				element->tryDraw(clipSpace);
			}
		}

		void setScene(Scene* manager) override{
			Element::setScene(manager);
			for (const auto & element : getChildren()){
				element->setScene(manager);
			}
		}

		bool resize(const Geom::Vec2 size) override{
			if(Element::resize(size)){
				layout();
				return true;
			}
			return false;
		}

		bool updateAbsSrc(const Geom::Vec2 parentAbsSrc) override{
			if(Element::updateAbsSrc(parentAbsSrc)){
				for (const auto & element : getChildren()){
					element->updateAbsSrc(parentAbsSrc);
				}
				return true;
			}
			return false;
		}

	protected:
		void modifyChildren(Element* element){
			if(!element)throw std::invalid_argument("Cannot add null element");
			element->setScene(scene);
			element->setParent(this);
		}
	};

	//TODO move scene to other module interface, it is here only because msvc sucks

	struct SceneBase{
		struct MouseState{
			Geom::Vec2 src{};
			bool pressed{};

			void reset(const Geom::Vec2 pos){
				src = pos;
				pressed = true;
			}

			void clear(const Geom::Vec2 pos){

				src = pos;
				pressed = false;
			}
		};

		Geom::Vec2 pos{};
		Geom::Vec2 size{};

		Geom::Vec2 cursorPos{};
		std::array<MouseState, Ctrl::Mouse::Count> mouseKeyStates{};


		ext::dependency<ext::owner<Group*>> root{};

		//focus fields
		// Element* currentInputFocused{nullptr};
		Element* currentScrollFocus{nullptr};
		Element* currentCursorFocus{nullptr};

		std::vector<Element*> lastInbounds{};

		Graphic::RendererUI* renderer{};
		Bundle* bundle{};

		[[nodiscard]] SceneBase() = default;

		[[nodiscard]] SceneBase(const ext::owner<Group*> root, Graphic::RendererUI* renderer, Bundle* bundle)
			: root{root},
			  renderer{renderer},
			  bundle{bundle}{}

		SceneBase(const SceneBase& other) = delete;

		SceneBase(SceneBase&& other) noexcept = default;

		SceneBase& operator=(const SceneBase& other) = delete;

		SceneBase& operator=(SceneBase&& other) noexcept = default;
	};

	export struct Scene : SceneBase{

		[[nodiscard]] Scene() = default;

		[[nodiscard]] explicit Scene(
			ext::owner<Group*> root,
			Graphic::RendererUI* renderer = nullptr,
			Bundle* bundle = nullptr);

		~Scene();


		[[nodiscard]] bool isMousePressed() const noexcept{
			return std::ranges::any_of(mouseKeyStates, std::identity{}, &MouseState::pressed);
		}

		void dropAllFocus(const Element* target);

		std::vector<Element*> dfsFindDeepestElement(Element* target) const;

		void trySwapFocus(Element* newFocus);

		void swapFocus(Element* newFocus);

		void onMouseAction(int key, int action, int mode);

		void onKeyAction(int key, int action, int mode) const;

		void onTextInput(int code, int mode);

		void onScroll(Geom::Vec2 scroll) const;

		void onCursorPosUpdate(Geom::Vec2 newPos);

		void resize(Geom::Vec2 size);

		void update(float delta_in_ticks) const;

		void layout() const;

		void draw() const;

		Scene(const Scene& other) = delete;

		Scene(Scene&& other) noexcept;

		Scene& operator=(const Scene& other) = delete;

		Scene& operator=(Scene&& other) noexcept;

	private:
		void updateInbounds(std::vector<Element*>&& next);
	};
}

module : private;

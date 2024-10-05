//
// Created by Matrix on 2024/9/15.
//

export module Core.UI.ScrollPanel;

export import Core.UI.Group;
import std;
import ext.snap_shot;
import Math;

export namespace Core::UI{
	struct ScrollPanel : public Core::UI::Group{
		static constexpr auto TypeName = "ScrollPanel";
		ElementUniquePtr item{/*Should Be Not Null when using*/};

		static constexpr float VelocitySensitivity = 0.95f;
		static constexpr float VelocityDragSensitivity = 0.15f;
		static constexpr float VelocityScale = 20.f;

		float scrollBarStroke{20.0f};

		Geom::Vec2 scrollVelocity{};
		Geom::Vec2 scrollTargetVelocity{};
		ext::snap_shot<Geom::Vec2> scroll{};

		[[nodiscard]] std::span<const ElementUniquePtr> getChildren() const noexcept override{
			return item ? std::span{&item, 1} : std::span{static_cast<const ElementUniquePtr*>(nullptr), 0};
		}

		[[nodiscard]] ScrollPanel() : Group{TypeName}{
			events().on<Event::Drag>([this](const Event::Drag& e){
				scrollTargetVelocity = scrollVelocity = {};
				const auto trans = e.trans() * getVelClamp();
				const auto blank = getViewportSize() - Geom::Vec2{horiBarLength(), vertBarSLength()};

				auto rst = scroll.base + (trans / blank) * getScrollableSize();

				//clear NaN
				if(!enableHoriScroll())rst.x = 0;
				if(!enableVertScroll())rst.y = 0;

				auto [maxX, maxY] = getScrollableSize();
				rst.clampXY({0, -maxY}, {maxX, 0});

				if(Util::tryModify(scroll.temp, rst)){
					updateChildrenAbsSrc();
				}
			});

			events().on<Event::Click>([this](const Event::Click& e){
				if(e.code.action() == Ctrl::Act::Release){
					scroll.apply();
				}
			});

			events().on<Event::Scroll>([this](const Event::Scroll& e){
				auto cmp = e.pos;

				if(e.mode == Ctrl::Mode::Shift){
					cmp.swapXY();
				}

				scrollTargetVelocity = cmp * getVelClamp();
				scrollVelocity = scrollTargetVelocity.scl(VelocityScale, VelocityScale);
			});

			events().on<Event::Inbound>([this](const auto& e){
				setFocusedScroll(true);
			});

			events().on<Event::Exbound>([this](const auto& e){
				setFocusedScroll(false);
			});


			events().on<Event::BeginFocus>([this](const auto& e){
				setFocusedScroll(true);
			});

			events().on<Event::EndFocus>([this](const auto& e){
				setFocusedScroll(false);
			});

			property.maintainFocusUntilMouseDrop = true;
			layoutState.ignoreChildren();
		}

		void update(const float delta_in_ticks) override{
			Group::update(delta_in_ticks);

			scrollVelocity.lerp(scrollTargetVelocity, delta_in_ticks * VelocitySensitivity);
			scrollTargetVelocity.lerp(Geom::zeroVec2<float>, delta_in_ticks * VelocityDragSensitivity);

			auto [maxX, maxY] = getScrollableSize();

			if(Util::tryModify(
				scroll.base,
					scroll.base.copy().addScaled(scrollVelocity, delta_in_ticks).clampXY({0, -maxY}, {maxX, 0}) * getVelClamp())){
				scroll.resume();
				updateChildrenAbsSrc();
			}

			item->update(delta_in_ticks);
		}

		bool updateAbsSrc(const Geom::Vec2 parentAbsSrc) override{
			if(Element::updateAbsSrc(parentAbsSrc)){
				updateChildrenAbsSrc();
				return true;
			}
			return false;
		}

		bool resize(const Geom::Vec2 size) override{
			if(Element::resize(size)){
				setItemSize();

				scroll.resume();
				updateChildrenAbsSrc();

				return true;
			}
			return false;
		}

		void setItem(ElementUniquePtr&& item){
			this->item = std::move(item);
			modifyChildren(*this->item.get());
			setItemSize();
		}

		template <Geom::Vector2D<bool> fillParent = {true, false}, ElemInitFunc Fn>
			requires (std::is_default_constructible_v<typename ElemInitFuncTraits<Fn>::ElemType>)
		void setItem(Fn&& init){
			this->item = ElementUniquePtr{this, scene, [&](typename ElemInitFuncTraits<Fn>::ElemType& e){
				ScrollPanel::modifyChildren<fillParent>(e);
				init(e);
			}};

			setItemSize();
		}

		void postRemove(Element* element) override{
			throw std::runtime_error("Not implemented by ScrollPanel");
		}

		void instantRemove(Element* element) override{
			throw std::runtime_error("Not implemented by ScrollPanel");
		}

		Element& addChildren(ElementUniquePtr&& element) override{
			throw std::runtime_error("Not implemented by ScrollPanel");
		}

		void drawMain() const override;
		void drawPost() const override;

		[[nodiscard]] bool containsPos_parent(const Geom::Vec2 cursorPos) const override{
			return (!parent || Rect{Geom::FromExtent, absPos(), getViewportSize()}.containsPos_edgeExclusive(cursorPos));
		}

	private:
		template <Geom::Vector2D<bool> fillParent = {true, false}>
		static void modifyChildren(Element& element){
			element.prop().fillParent = fillParent;
		}

		void setItemSize() const{
			setChildrenFillParentSize(*item, getViewportSize());
		}

		void updateChildrenAbsSrc() const{
			auto offset = -scroll.temp;
			offset.y -= (getItemSize().y - prop().getValidHeight());
			item->prop().relativeSrc = offset;
			item->updateAbsSrc(absPos());
		}

		[[nodiscard]] constexpr Geom::Vec2 getVelClamp() const noexcept{
			return Geom::Vector2D{enableHoriScroll(), enableVertScroll()}.as<float>();
		}

		[[nodiscard]] constexpr bool enableHoriScroll() const noexcept{
			return getItemSize().x > property.getValidWidth();
		}

		[[nodiscard]] constexpr bool enableVertScroll() const noexcept{
			return getItemSize().y > property.getValidHeight();
		}

		[[nodiscard]] constexpr Geom::Vec2 getScrollableSize() const noexcept{
			return (getItemSize() - getViewportSize()).max(Geom::zeroVec2<float>);
		}

		[[nodiscard]] constexpr Geom::NorVec2 getScrollProgress(const Geom::Vec2 pos) const noexcept{
			return pos / getScrollableSize();
		}

		[[nodiscard]] constexpr Geom::Vec2 getItemSize() const noexcept{
			return item->getSize();
		}

		[[nodiscard]] constexpr Geom::Vec2 getBarSize() const noexcept{
			Geom::Vec2 rst{};

			if(enableHoriScroll())rst.y += scrollBarStroke;
			if(enableVertScroll())rst.x += scrollBarStroke;

			return rst;
		}

		[[nodiscard]] float horiBarLength() const {
			const auto w = getViewportSize().x;
			return Math::clampPositive(Math::min(w / getItemSize().x, 1.0f) * w);
		}

		[[nodiscard]] float vertBarSLength() const {
			const auto h = getViewportSize().y;
			return Math::clampPositive(Math::min(h / getItemSize().y, 1.0f) * h);
		}


		[[nodiscard]] constexpr Geom::Vec2 getViewportSize() const noexcept{
			return getValidSize() - getBarSize();
		}

		[[nodiscard]] Rect getHoriBarRect() const {
			const auto [x, y] = absPos() + property.boarder.bot_lft();
			const auto ratio = getScrollProgress(scroll.temp);
			const auto barSize = getBarSize();
			const auto width = horiBarLength();
			return {
				x + ratio.x * (property.getValidWidth() - barSize.x - width),
				y,
				width,
				barSize.y
			};
		}

		[[nodiscard]] Rect getVertBarRect() const {
			const auto [x, y] = absPos() - property.boarder.top_rit() + getSize();
			const auto ratio = getScrollProgress(scroll.temp);
			const auto barSize = getBarSize();
			const auto height = vertBarSLength();
			return {
				x - barSize.x,
				y + ratio.y * (property.getValidHeight() - vertBarSLength() - barSize.y) - height,
				barSize.x,
				height
			};
		}
	};

	static_assert(std::is_default_constructible_v<ScrollPanel>);
}

//
// Created by Matrix on 2024/9/24.
//

export module Core.UI.TextElement;

export import Core.UI.Element;
export import Core.UI.StatedLength;
import Core.UI.Scene;

import Font.TypeSettings;
import std;
import Math;

export namespace Core::UI{
	struct TextElement : Element {
		const Font::TypeSettings::Parser* parser{&Font::TypeSettings::globalInstantParser};

		Geom::Vector2D<bool> glyphSizeDependency{};
		Align::Pos textAlignMode{Align::Pos::top_left};

	protected:
		/** @brief should be not null*/
		std::shared_ptr<Font::TypeSettings::GlyphLayout> glyphLayout{std::make_shared<Font::TypeSettings::GlyphLayout>()};
		std::future<void> layoutTasks{};
		bool textChanged{};

		float textScale{1.f};

	public:
		[[nodiscard]] TextElement() : Element{"TextElement"}{
			interactivity = Interactivity::disabled;
		}

		void setTextScale(const float scl){
			if(!Math::equal(scl, textScale)){
				textScale = scl;

				if(!textChanged){
					getScene()->registerIndependentLayout(this);
					textChanged = true;
				}

			}
		}
		[[nodiscard]] std::string_view getText() const noexcept{
			return glyphLayout->text;
		}

		void setText(const std::string_view text) /*noexcept*/{
			const auto sz = getTextBoundSize();
			if(glyphLayout->isLayoutChanged(text, sz)){
				glyphLayout->reset<false>(std::string{text}, sz);
				textChanged = true;
				getScene()->registerIndependentLayout(this);
				notifyLayoutChanged(SpreadDirection::upper);
			}
		}

		bool resize(const Geom::Vec2 size) override{
			if(property.clampedSize.setSize(size)){
				updateTextSize();
				notifyLayoutChanged(SpreadDirection::all | SpreadDirection::child_item);
				return true;
			}
			return false;
		}

		void tryLayout() override{
			if(layoutState.isChanged() || textChanged){
				layout();
			}
		}

		void layout() override{
			auto sz = layoutText();
			if(!layoutState.isRestricted())resize(sz + property.boarder.getSize());

			Element::layout();
		}

		void drawMain() const override;

		[[nodiscard]] Geom::Vec2 requestSpace(const StatedSize sz) override{
			Geom::Vec2 baseSize{};
			if(sz.x.mastering())baseSize.x = sz.x.val;
			if(sz.y.mastering())baseSize.y = sz.y.val;

			auto [rx, ry] = getRestriction();

			if(sz.x.external() != rx || sz.y.external() != ry){
				throw std::logic_error("incompatible text dependency size");
			}

			auto validSize = property.evaluateValidSize(baseSize);

			validSize.x = rx ? std::numeric_limits<float>::max() : validSize.x;
			validSize.y = ry ? std::numeric_limits<float>::max() : validSize.y;

			if(glyphLayout->resetSize<false>(getTextBoundSize()) || rx || ry){
				const auto rstSize = layoutText();
				return rstSize + property.boarder.getSize();
			}else{
				return getSize();
			}
		}

	protected:
		Geom::Vec2 layoutText(){
			parser->parse(glyphLayout, [this](Font::TypeSettings::FormatContext& context){
				context.setParseScale(textScale);
			});
			glyphLayout->align = Align::Pos::top_left;
			textChanged = false;

			const auto layoutSize = glyphLayout->getDrawSize();
			const auto validSize = getValidSize();

			const Geom::Vec2 sz{
				getRestriction().x ? layoutSize.x : validSize.x,
				getRestriction().y ? layoutSize.y : validSize.y,
			};

			return sz;
		}

		void updateTextSize(){
			if(glyphLayout->resetSize<false>(getTextBoundSize())){
				getScene()->registerIndependentLayout(this);
				textChanged = true;
			}
		}

		[[nodiscard]] constexpr Geom::Vec2 getTextBoundSize() const noexcept{
			return {
				getRestriction().x ? std::numeric_limits<float>::max() : property.getValidWidth(),
				getRestriction().y ? std::numeric_limits<float>::max() : property.getValidHeight(),
			};
		}

		[[nodiscard]] constexpr Geom::Vector2D<bool> getRestriction() const noexcept{
			return {
				glyphSizeDependency.x && !layoutState.restrictedByParent,
				glyphSizeDependency.y && !layoutState.restrictedByParent,
			};
		}

		[[nodiscard]] constexpr Geom::Vec2 getTextOffset() const noexcept{
			const auto sz = glyphLayout->getDrawSize();
			const auto rst = Align::getOffsetOf(textAlignMode, sz, property.getValidBound_absolute());

			return rst;
		}
	};
}

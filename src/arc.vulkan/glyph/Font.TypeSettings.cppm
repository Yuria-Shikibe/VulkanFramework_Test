//
// Created by Matrix on 2024/8/26.
//

export module Font.TypeSettings;

export import Font;
export import Font.GlyphToRegion;

import Align;

import Graphic.ImageRegion;
import Graphic.Color;
import ext.SnapShot;
import ext.Heterogeneous;
import ext.TaskQueue;

import Geom.Rect_Orthogonal;
import Geom.Vector2D;
import Math;

import std;
import ext.Encoding;
import ext.Concepts;

namespace Font{
	export using TextLayoutPos = Geom::Point2US;

	export inline FontManager* GlobalFontManager{nullptr};

	export inline ext::StringHashMap<FontFaceID> namedFonts{};

	//TODO builtin fontsizes
	export inline ext::StringHashMap<GlyphSizeType> fontSize{};

	export namespace TypeSettings{
		constexpr GlyphSizeType DefaultSize{0, 48};
		constexpr Graphic::Color DefaultColor{Graphic::Colors::WHITE};

		// using T = int;
		// using Cont = std::vector<T>;
		template <typename T, typename Cont = std::vector<T>>
		struct OptionalStack{
			std::stack<T, Cont> stack{};

			[[nodiscard]] OptionalStack() = default;

			[[nodiscard]] explicit OptionalStack(const std::stack<T, Cont>& stack)
				: stack{stack}{}

			void push(const T& val){
				stack.push(val);
			}

			void push(T&& val){
				stack.push(std::move(val));
			}

			std::optional<T> popAndGet(){
				if(stack.empty()){
					return std::nullopt;
				}

				const std::optional rst{std::move(stack.top())};
				stack.pop();
				return rst;
			}

			void pop(){
				if(!stack.empty()) stack.pop();
			}

			[[nodiscard]] T top(const T defaultVal) const noexcept{
				if(stack.empty()){
					return defaultVal;
				} else{
					return stack.top();
				}
			}
		};

		struct Element{
			CharCode code{};
			std::uint32_t index{};
			TextLayoutPos layoutPos{};

			const Glyph* glyph{nullptr};
			Graphic::Color fontColor{};

			Geom::Vec2 src{};
			Geom::Vec2 end{};

			Math::Range heightAlign{};

			[[nodiscard]] constexpr Geom::Vec2 v00() const noexcept{
				return src;
			}

			[[nodiscard]] constexpr Geom::Vec2 v01() const noexcept{
				return {src.x, end.y};
			}

			[[nodiscard]] constexpr Geom::Vec2 v10() const noexcept{
				return {end.x, src.y};
			}

			[[nodiscard]] constexpr Geom::Vec2 v11() const noexcept{
				return end;
			}
		};

		/**
		 * @warning If any tokens are used, the lifetime of @link FormattableText @endlink should not longer than the source string
		 */
		struct FormattableText{
			static constexpr char TokenSplitChar = '|';
			struct TokenSentinel{
				char tokenSignal{'#'};
				char tokenBegin{'<'};
				char tokenEnd{'>'};
				bool reserve{false};
			};

			std::uint32_t rows{};
			/**
			 * @brief String Normalize to NFC Format
			 */
			std::basic_string<CharCode> codes{};
			using PosType = decltype(codes)::size_type;

			struct TokenArgument{
				std::uint32_t pos{};
				std::string_view data{};

				[[nodiscard]] decltype(auto) getAllArgs() const{
					return data
						| std::views::split(TokenSplitChar)
						| std::views::transform([](auto a){ return std::string_view{a}; });
				}

				[[nodiscard]] decltype(auto) getRestArgs() const{
					return getAllArgs() | std::views::drop(1);
				}

				[[nodiscard]] std::string_view getName() const{
					return getAllArgs().front();
				}

				[[nodiscard]] PosType beginPos() const{
					return pos;
				}
			};

			std::vector<TokenArgument> tokens{};

			using TokenItr = decltype(tokens)::iterator;

			TokenItr getToken(const PosType pos, const TokenItr& last){
				return std::ranges::lower_bound(last, tokens.end(), pos, std::less<>{}, &TokenArgument::beginPos);
			}

			TokenItr getToken(const PosType pos){
				return getToken(pos, tokens.begin());
			}

			[[nodiscard]] decltype(auto) getTokenGroup(const PosType pos, const TokenItr& last) const{
				return std::ranges::equal_range(last, tokens.end(), pos, std::less<>{}, &TokenArgument::beginPos);
			}

			[[nodiscard]] FormattableText() = default;

			[[nodiscard]] explicit(false) FormattableText(std::string_view string, TokenSentinel sentinel = {});
		};

		struct GlyphRect{
			float width{};
			float ascender{};
			float descender{};

			[[nodiscard]] constexpr float height() const noexcept{
				return ascender + descender;
			}

			[[nodiscard]] constexpr Geom::Vec2 size() const noexcept{
				return {width, height()};
			}
		};

		struct Context{
			FormattableText text{};

			OptionalStack<Graphic::Color> colorHistory{};
			OptionalStack<Geom::Vec2> offsetHistory{};
			OptionalStack<GlyphSizeType> sizeHistory{};
			OptionalStack<IndexedFontFace*> fontHistory{};

			Geom::Vec2 maxSize{};

			Geom::Vec2 penPos{};
			Geom::Vec2 currentOffset{};

			GlyphRect lineRect{};


			float heightRemain{};

			float minimumLineHeight{48.f};
			float lineSpacing{12.f};
			float paragraphSpacing{};

			[[nodiscard]] GlyphSizeType getLastSize() const{
				return sizeHistory.top(DefaultSize);
			}

			void pushOffset(const Geom::Vec2 off){
				offsetHistory.push(off);
				currentOffset += off;
			}

			void pushScaledSize(const float scl){
				sizeHistory.push(getLastSize().scl(scl));
			}

			void pushScaledOffset(const Geom::Vec2 offScale){
				auto off = getLastSize();
				if(off.x == 0) off.x = off.y;
				if(off.y == 0) off.y = off.x;

				const auto offset = off.as<float>() * offScale;
				offsetHistory.push(offset);
				currentOffset += offset;
			}

			Geom::Vec2 popOffset(){
				const Geom::Vec2 offset = offsetHistory.popAndGet().value_or(Geom::Vec2{});
				currentOffset -= offset;
				return offset;
			}

			[[nodiscard]] IndexedFontFace& getFace() const{
				const auto t = fontHistory.top(GlobalFontManager->fontFaces_fastAccess.front());
				if(t == nullptr) throw std::runtime_error("No Valid Font Face");
				return *t;
			}

			[[nodiscard]] Glyph& getGlyph(const CharCode code) const{
				return getFace().obtainGlyph({code, getLastSize()}, GlobalFontManager->atlas, GlobalFontManager->fontPage);
			}
		};

		struct Layout{
			struct Row{
				float baselineHeight{};
				Geom::Vec2 src{};
				GlyphRect bound{};
				std::vector<Element> glyphs{};

				[[nodiscard]] Geom::OrthoRectFloat getRectBound() const noexcept{
					return {src.x, src.y - bound.descender, bound.width, bound.height()};
				}
			};

			Geom::Vec2 minimumSize{};
			Geom::Vec2 size{};
			Align::Pos align{};

			float drawScale{};
			bool isCompressed{};

			void setAlign(Align::Pos align){
				std::unique_lock lk{capture};
				if(this->align == align) return;
				this->align = align;

				updateAlign();
			}

			void updateAlign(){
				if(align & Align::Pos::center_x){
					for(auto& element : elements){
						element.src.x = (size.x - element.bound.width) / 2.f;
					}
				} else if(align & Align::Pos::left){
					for(auto& element : elements){
						element.src.x = 0;
					}
				} else if(align & Align::Pos::right){
					for(auto& element : elements){
						element.src.x = (size.x - element.bound.width);
					}
				}
			}

			Geom::Vec2 maximumSize{};
			std::string text{};

			std::vector<Row> elements{};

			std::shared_mutex capture{};

			[[nodiscard]] Layout() = default;

			[[nodiscard]] bool requiresLayout(const std::string_view text, const Geom::Vec2 size) const{
				return size != maximumSize || this->text != text;
			}

			void reset(std::string&& text, const Geom::Vec2 size){
				std::unique_lock lk{capture};
				std::ranges::for_each(elements, &std::vector<Element>::clear, &Row::glyphs);
				this->text = std::move(text);
				maximumSize = size;
			}
		};

		struct TokenModifier{
			using FuncType = void(const std::vector<std::string_view>&, Context&, const std::shared_ptr<Layout>&);
			std::function<FuncType> modifier{};

			void operator()(const std::vector<std::string_view>& tokens, Context& context,
			                const std::shared_ptr<Layout>& target) const{
				modifier(tokens, context, target);
			}

			[[nodiscard]] TokenModifier() = default;

			//
			[[nodiscard]] explicit(false) TokenModifier(std::function<FuncType>&& modifier)
				: modifier{std::move(modifier)}{}

			template <Concepts::Invokable<FuncType> Func>
			[[nodiscard]] explicit(false) TokenModifier(Func&& modifier)
				: modifier{std::forward<Func>(modifier)}{}
		};

		struct Parser{
			// ext::StringHashMap<>

			ext::TaskQueue<void()> taskQueue{};

			ext::StringHashMap<TokenModifier> modifiers{};

			void parse(Context& context, const std::shared_ptr<Layout>& target) const;

			struct ParseInstance{
				const Parser* parser{};
				std::weak_ptr<Layout> layout{};
				mutable Context context{};

				void operator()() const{
					const auto ptr = layout.lock();
					if(!ptr) return;

					parser->parse(context, ptr);
				}
			};

			[[nodiscard]] std::optional<decltype(taskQueue)::Future> requestParse(
				const std::shared_ptr<Layout>& target,
				std::string&& text,
				const Geom::Vec2 size = Geom::maxVec2<float>){
				if(target->requiresLayout(text, size)){
					target->reset(std::move(text), size);
					return {taskQueue.push(ParseInstance{this, target})};
				}

				return std::nullopt;
			}

			void requestParseInstantly(
				const std::shared_ptr<Layout>& target,
				std::string&& text,
				const Geom::Vec2 size = Geom::maxVec2<float>) const{
				if(target->requiresLayout(text, size)){
					target->reset(std::move(text), size);
					ParseInstance{this, target}.operator()();
				}
			}
		};
	}


	namespace TypeSettings{
		namespace Func{
			export template <typename T>
				requires std::is_arithmetic_v<T>
			T string_cast(std::string_view str, T def = 0){
				T t{def};
				std::from_chars(str.data(), str.data() + str.size(), t);
				return t;
			}

			export template <typename T = int>
				requires std::is_arithmetic_v<T>
			std::vector<T> string_cast_seq(std::string_view str, T def = 0, std::size_t expected = 2){
				const char* begin = str.data();
				const char* end = begin + str.size();

				std::vector<T> result{};
				if(expected) result.reserve(expected);

				while(!expected || result.size() != expected){
					if(begin == end) break;
					T t{def};
					auto [ptr, ec] = std::from_chars(begin, end, t);
					begin = ptr;

					if(ec == std::errc::invalid_argument){
						begin++;
					} else{
						result.push_back(t);
					}
				}

				return result;
			}

			export void beginSubscript(Context& context, const std::shared_ptr<Layout>& target){
				context.pushScaledOffset({-0.025f, -0.105f});
				context.pushScaledSize(0.45f);
			}

			export void beginSuperscript(Context& context, const std::shared_ptr<Layout>& target){
				context.pushScaledOffset({-0.025f, 0.525f});
				context.pushScaledSize(0.45f);
			}

			export void endScript(Context& context, const std::shared_ptr<Layout>& target){
				context.popOffset();
				context.sizeHistory.pop();
			}

			bool endLine(Context& context, const std::shared_ptr<Layout>& target){
				float lastLineDescender;

				if(target->elements.size() > 1){
					auto& lastLine = std::next(target->elements.crbegin()).operator*();
					lastLineDescender = lastLine.bound.descender;
				} else{
					lastLineDescender = -context.lineSpacing;
				}

				auto& line = target->elements.back();
				const float height = Math::max(context.minimumLineHeight,
				                               context.lineRect.ascender + lastLineDescender + context.lineSpacing);

				line.src.y -= height;
				context.maxSize.y += height;
				if(context.maxSize.y > target->maximumSize.y){
					//discard line
					target->elements.pop_back();
					context.maxSize.y = target->maximumSize.y;
					return false;
				}

				line.bound = context.lineRect;

				context.penPos.x = 0;
				context.penPos.y -= height;
				context.heightRemain -= height;
				context.lineRect = {};

				context.maxSize.maxX(line.bound.width);

				return true;
			}

			void beginParse(Context& context, const std::shared_ptr<Layout>& target){
				target->elements.emplace_back();
				context.heightRemain = target->maximumSize.y;
			}

			void endParse(Context& context, const std::shared_ptr<Layout>& target){
				if(target->elements.empty()) return;
				// context.maxSize.y += target->elements.front().bound.ascender;
				context.maxSize.y += target->elements.back().bound.descender;

				target->size = context.maxSize;
				target->size.max(target->minimumSize);

				if(!(target->align & Align::Pos::left)) target->updateAlign();
				for(auto&& element : target->elements){
					element.src.y += target->size.y;
				}
			}

			void execTokens(
				const Parser& parser,
				FormattableText::TokenItr& lastTokenItr,
				const FormattableText& formattableText,
				const std::string_view::size_type index,
				Context& context,
				const std::shared_ptr<Layout>& target){
				auto&& tokens = formattableText.getTokenGroup(index, lastTokenItr);
				lastTokenItr = tokens.end();

				for(const auto& token : tokens){
					if(const auto tokenParser = parser.modifiers.tryFind(token.getName())){
						tokenParser->operator()(token.getRestArgs() | std::ranges::to<std::vector>(), context, target);
					} else{
						std::println(std::cerr, "[Parser] Failed To Find Token: {}", token.data);
					}
				}
			}

			Layout::Row& getCurrentRow(const std::size_t row, const Context& context, const std::shared_ptr<Layout>& target){
				if(row >= target->elements.size()){
					auto& newLine = target->elements.emplace_back();
					newLine.src = context.penPos;
					return newLine;
				}

				return target->elements[row];
			}

			Parser createDefParser();
		}


		export inline Parser globalParser = Func::createDefParser();
		export const inline Parser globalInstantParser = Func::createDefParser();
	}
}

module : private;

Font::TypeSettings::Parser Font::TypeSettings::Func::createDefParser(){
	{
		Parser parser;
		parser.modifiers["size"] = [](const std::vector<std::string_view>& args, Context& context,
		                              const std::shared_ptr<Layout>& target){
			if(args.empty()){
				context.sizeHistory.pop();
				return;
			}

			if(args.front().starts_with('[')){
				const auto size = Func::string_cast_seq<std::uint16_t>(args.front().substr(1), 0, 2);

				switch(size.size()){
					case 1 : context.sizeHistory.push({0, size[0]});
						break;
					case 2 : context.sizeHistory.push({size[0], size[1]});
						break;
					default : context.sizeHistory.pop();
				}
			}
		};

		parser.modifiers["scl"] = [](const std::vector<std::string_view>& args, Context& context,
		                             const std::shared_ptr<Layout>& target){
			if(args.empty()){
				context.sizeHistory.pop();
				return;
			}

			const auto scale = Func::string_cast<float>(args.front());
			context.sizeHistory.push(context.getLastSize().scl(scale));
		};

		parser.modifiers["s"] = parser.modifiers["size"];

		parser.modifiers["color"] = [](const std::vector<std::string_view>& args, Context& context,
		                               const std::shared_ptr<Layout>& target){
			if(args.empty()){
				context.colorHistory.pop();
				return;
			}

			if(args.size() > 1){
				context.colorHistory.pop();
			}

			if(std::string_view arg = args.front(); arg.starts_with('[')){
				arg.remove_prefix(1);

				if(arg.empty()){
					context.colorHistory.pop();
				} else{
					const auto color = Graphic::Color::valueOf(arg);
					context.colorHistory.push(color);
				}
			}
		};

		parser.modifiers["c"] = parser.modifiers["color"];

		parser.modifiers["off"] = [](const std::vector<std::string_view>& args, Context& context,
		                             const std::shared_ptr<Layout>& target){
			if(args.empty()){
				context.popOffset();
				return;
			}

			const auto offset = Func::string_cast_seq<float>(args.front(), 0, 2);

			switch(offset.size()){
				case 1 : context.pushOffset({0, offset[0]});
					break;
				case 2 : context.pushOffset({offset[0], offset[1]});
					break;
				default : context.popOffset();
			}
		};

		parser.modifiers["offs"] = [](const std::vector<std::string_view>& args, Context& context,
		                              const std::shared_ptr<Layout>& target){
			if(args.empty()){
				context.popOffset();
				return;
			}

			const auto offset = Func::string_cast_seq<float>(args.front(), 0, 2);

			switch(offset.size()){
				case 1 : context.pushScaledOffset({0, offset[0]});
					break;
				case 2 : context.pushScaledOffset({offset[0], offset[1]});
					break;
				default : context.popOffset();
			}
		};

		parser.modifiers["font"] = [](const std::vector<std::string_view>& args, Context& context,
		                              const std::shared_ptr<Layout>& target){
			if(args.empty()){
				context.fontHistory.pop();
				return;
			}

			Font::FontFaceID faceId{};
			if(const std::string_view arg = args.front(); arg.starts_with('[')){
				faceId = Func::string_cast<Font::FontFaceID>(arg.substr(1));
			} else{
				faceId = Font::namedFonts.at(arg, 0);
			}

			if(auto* font = Font::GlobalFontManager->getFontFace(faceId)){
				context.fontHistory.push(font);
			}
		};

		parser.modifiers["_"] = parser.modifiers["sub"] = [](const std::vector<std::string_view>& args,
		                                                     Context& context,
		                                                     const std::shared_ptr<Layout>& target){
			Func::beginSubscript(context, target);
		};

		parser.modifiers["^"] = parser.modifiers["sup"] = [](const std::vector<std::string_view>& args,
		                                                     Context& context,
		                                                     const std::shared_ptr<Layout>& target){
			Func::beginSuperscript(context, target);
		};

		parser.modifiers["\\"] = parser.modifiers["\\sup"] = parser.modifiers["\\sub"] = [](
			const std::vector<std::string_view>& args, Context& context, const std::shared_ptr<Layout>& target){
				Func::endScript(context, target);
			};

		return parser;
	}
}

Font::TypeSettings::FormattableText::FormattableText(const std::string_view string, const TokenSentinel sentinel){
	static constexpr auto InvalidPos = std::string_view::npos;
	using namespace Font;
	const auto size = string.size();

	codes.reserve(string.size() + 1);

	enum struct TokenState{
		waiting,
		endWaiting,
		signaled
	};

	bool escapingNext{};
	TokenState recordingToken{};
	decltype(string)::size_type tokenRegionBegin{InvalidPos};
	TokenArgument* currentToken{};

	for(decltype(string)::size_type off = 0; off < size; ++off){
		const char codeByte = string[off];
		CharCode charCode{reinterpret_cast<const std::uint8_t&>(codeByte)};

		if(charCode == '\r') continue;

		if(charCode == '\\' && recordingToken != TokenState::signaled){
			if(!escapingNext){
				escapingNext = true;
				continue;
			} else{
				escapingNext = false;
			}
		}

		if(!escapingNext){
			if(codeByte == sentinel.tokenSignal && tokenRegionBegin == InvalidPos){
				if(recordingToken != TokenState::signaled){
					recordingToken = TokenState::signaled;
				} else{
					recordingToken = TokenState::waiting;
				}

				if(recordingToken == TokenState::signaled){
					currentToken = &tokens.emplace_back(TokenArgument{static_cast<std::uint32_t>(codes.size())});

					if(!sentinel.reserve) continue;
				}
			}

			if(recordingToken == TokenState::endWaiting){
				if(codeByte == sentinel.tokenBegin){
					recordingToken = TokenState::signaled;
					currentToken = &tokens.emplace_back(TokenArgument{static_cast<std::uint32_t>(codes.size())});
				} else{
					recordingToken = TokenState::waiting;
				}
			}

			if(recordingToken == TokenState::signaled){
				if(codeByte == sentinel.tokenBegin && tokenRegionBegin == InvalidPos){
					tokenRegionBegin = off + 1;
				}

				if(codeByte == sentinel.tokenEnd){
					if(!currentToken || tokenRegionBegin == InvalidPos){
						throw std::invalid_argument("Parse String Tokens Failed");
					}

					currentToken->data = string.substr(tokenRegionBegin, off - tokenRegionBegin);
					tokenRegionBegin = InvalidPos;
					recordingToken = TokenState::endWaiting;
				}

				if(!sentinel.reserve) continue;
			}
		}

		const auto codeSize = ext::encode::getUnicodeLength<>(reinterpret_cast<const char&>(charCode));

		if(!escapingNext){
			if(codeSize > 1 && off + codeSize <= size){
				charCode = ext::encode::convertTo(string.data() + off, codeSize);
			}

			codes.push_back(charCode);
		} else{
			escapingNext = false;
		}

		if(charCode == '\n') rows++;

		off += codeSize - 1;
	}

	if(codes.ends_with(U'\n')){
		codes.back() = '\0';
	} else{
		codes.push_back(U'\0');
		rows++;
	}

	codes.shrink_to_fit();
}

void Font::TypeSettings::Parser::parse(Context& context, const std::shared_ptr<Layout>& target) const{
	target->elements.clear();
	target->size = {};

	FormattableText formattableText{target->text, {}};

	std::unique_lock lk{target->capture};
	target->elements.reserve(formattableText.rows + 16);

	FormattableText::TokenItr lastTokenItr = formattableText.tokens.begin();
	std::uint32_t currentRow{};

	Func::beginParse(context, target);

	auto view = formattableText.codes | std::views::enumerate;

	auto endline = [&]{
		if(Func::endLine(context, target)){
			currentRow++;
			return true;
		} else{
			target->isCompressed = true;
			return false;
		}
	};

	for(auto itr = view.begin(); itr != view.end(); ++itr){
		auto [index, code] = *itr;

		Func::execTokens(*this, lastTokenItr, formattableText, index, context, target);

		auto& line = Func::getCurrentRow(currentRow, context, target);
		const auto linePos = line.glyphs.size();

		const Glyph& glyph = context.getGlyph(code);

		if(glyph.metrics.size.y > context.heightRemain){
			endline();
			target->isCompressed = true;
			break;
		}

		if(context.lineRect.width + glyph.metrics.advance.x > target->maximumSize.x){
			if(code != U'\n') --itr;
			if(!endline()) break;
			continue;
		}

		auto& current = line.glyphs.emplace_back();
		current.glyph = &glyph;

		const Geom::Vec2 localPos = context.penPos - line.src + context.currentOffset;

		auto [src, end] = glyph.metrics.placeTo(localPos);

		current.code = code;
		current.src = src;
		current.end = end;
		current.fontColor = context.colorHistory.top(DefaultColor);
		current.index = index;
		current.layoutPos = TextLayoutPos{static_cast<unsigned short>(currentRow), static_cast<unsigned short>(linePos)};

		context.lineRect.width += glyph.metrics.advance.x;
		context.lineRect.ascender = Math::max(context.lineRect.ascender, glyph.metrics.ascender());
		context.lineRect.descender = Math::max(context.lineRect.descender, glyph.metrics.descender());

		context.penPos.addX(glyph.metrics.advance.x);

		if(code == U'\n' || code == 0){
			if(!endline()) break;
		}
	}

	Func::endParse(context, target);
}

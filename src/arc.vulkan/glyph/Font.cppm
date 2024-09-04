module;

#if DEBUG_CHECK
#define FT_CONFIG_OPTION_ERROR_STRINGS
#endif

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftstroke.h>

#include "../src/ext/enum_operator_gen.hpp"

export module Font;


import std;

import Geom.Rect_Orthogonal;
import Geom.Vector2D;

import Graphic.Pixmap;

import Math;

import ext.RuntimeException;
import ext.Heterogeneous;
import ext.TransferAdaptor;

namespace Font{
	export struct Library{
		FT_Library library{};

		[[nodiscard]] Library();

		~Library();

		constexpr explicit(false) operator FT_Library() const{
			return library;
		}

		Library(const Library& other) = delete;

		Library(Library&& other) noexcept
			: library{other.library}{
			other.library = nullptr;
		}

		Library& operator=(const Library& other) = delete;

		Library& operator=(Library&& other) noexcept{
			if(this == &other) return *this;
			library = other.library;
			other.library = nullptr;
			return *this;
		}
	};

	export inline const Library GlobalFreeTypeLib{};

	export using CharCode = char32_t;
	export using GlyphSizeType = Geom::Vector2D<std::uint16_t>;
	export using GlyphRawMetrics = FT_Glyph_Metrics;

	export using FontFaceID = std::uint32_t;

	export struct GlyphKey{
		CharCode code{};
		GlyphSizeType size{};

		constexpr bool operator==(const GlyphKey&) const = default;
	};


	export template <std::signed_integral T = int>
	constexpr T normalizeLen(const FT_Pos pos) noexcept {
		return static_cast<T>(pos >> 6);
	}

	export template <std::floating_point T = float>
	constexpr T normalizeLen(const FT_Pos pos) noexcept {
		return static_cast<T>(pos) / static_cast<T>(64.); // NOLINT(*-narrowing-conversions)
	}

	export struct GlyphMetrics{
		Geom::Vec2 size{};
		Geom::Vec2 horiBearing{};
		Geom::Vec2 vertBearing{};
		Geom::Vec2 advance{};

		[[nodiscard]] GlyphMetrics() = default;

		[[nodiscard]] explicit(false) GlyphMetrics(const GlyphRawMetrics& metrics){
			size.x = normalizeLen<float>(metrics.width);
			size.y = normalizeLen<float>(metrics.height);
			horiBearing.x = normalizeLen<float>(metrics.horiBearingX);
			horiBearing.y = normalizeLen<float>(metrics.horiBearingY);
			advance.x = normalizeLen<float>(metrics.horiAdvance);
			vertBearing.x = normalizeLen<float>(metrics.vertBearingX);
			vertBearing.y = normalizeLen<float>(metrics.vertBearingY);
			advance.y = normalizeLen<float>(metrics.vertAdvance);
		}

		[[nodiscard]] float ascender() const noexcept{
			return horiBearing.y;
		}

		[[nodiscard]] float descender() const noexcept{
			return size.y - horiBearing.y;
		}


		/**
		 * @param pos Pen position
		 * @return [v00, v11]
		 */
		[[nodiscard]] std::tuple<Geom::Vec2, Geom::Vec2> placeTo(Geom::Vec2 pos) const{
			Geom::Vec2 src = pos;
			src.add(horiBearing.x, -descender());
			Geom::Vec2 end = pos;
			end.add(horiBearing.x + size.x, horiBearing.y);

			return {src, end};
		}
	};


	Graphic::Pixmap toPixmap(const FT_Bitmap& map);

	export enum struct FontFlag{
		SCALABLE         = FT_FACE_FLAG_SCALABLE,
		FIXED_SIZES      = FT_FACE_FLAG_FIXED_SIZES,
		FIXED_WIDTH      = FT_FACE_FLAG_FIXED_WIDTH,
		SFNT             = FT_FACE_FLAG_SFNT,
		HORIZONTAL       = FT_FACE_FLAG_HORIZONTAL,
		VERTICAL         = FT_FACE_FLAG_VERTICAL,
		KERNING          = FT_FACE_FLAG_KERNING,
		FAST_GLYPHS      = FT_FACE_FLAG_FAST_GLYPHS,
		MULTIPLE_MASTERS = FT_FACE_FLAG_MULTIPLE_MASTERS,
		GLYPH_NAMES      = FT_FACE_FLAG_GLYPH_NAMES,
		EXTERNAL_STREAM  = FT_FACE_FLAG_EXTERNAL_STREAM,
		HINTER           = FT_FACE_FLAG_HINTER,
		CID_KEYED        = FT_FACE_FLAG_CID_KEYED,
		TRICKY           = FT_FACE_FLAG_TRICKY,
		COLOR            = FT_FACE_FLAG_COLOR,
		VARIATION        = FT_FACE_FLAG_VARIATION,
		SVG              = FT_FACE_FLAG_SVG,
		SBIX             = FT_FACE_FLAG_SBIX,
		SBIX_OVERLAY     = FT_FACE_FLAG_SBIX_OVERLAY,
	};

	export enum struct StyleFlag{
		ITALIC = FT_STYLE_FLAG_ITALIC,
		BOLD = FT_STYLE_FLAG_BOLD,
	};

	export constexpr auto ASCII_CHARS = []() constexpr {
		constexpr std::size_t Size = '~' - ' ' + 1;
		const auto targetChars = std::ranges::views::iota(' ', '~' + 1) | std::ranges::to<std::vector<CharCode>>();
		std::array<CharCode, Size> charCodes{};

		for(auto [idx, charCode] : targetChars | std::views::enumerate){
			charCodes[idx] = charCode;
		}

		return charCodes;
	}();

	void check(FT_Error error);

	struct FontFace_Internal{
		FT_Face face{};

		[[nodiscard]] FontFace_Internal() = default;

		~FontFace_Internal();

		FontFace_Internal(const FontFace_Internal& other);

		FontFace_Internal(FontFace_Internal&& other) noexcept;

		FontFace_Internal& operator=(const FontFace_Internal& other);

		FontFace_Internal& operator=(FontFace_Internal&& other) noexcept;

		explicit FontFace_Internal(const char* path, const FT_Long index = 0){
			check(FT_New_Face(GlobalFreeTypeLib, path, index, &face));
		}

		FT_Face operator->() const{
			return face;
		}

		[[nodiscard]] FT_UInt indexOf(const CharCode code) const{
			return FT_Get_Char_Index(face, code);
		}

		[[nodiscard]] FT_Error loadGlyph(const FT_UInt index, const FT_Int32 loadFlags = FT_LOAD_DEFAULT) const{
			return FT_Load_Glyph(face, index, loadFlags);
		}

		[[nodiscard]] FT_Error loadChar(const FT_ULong code, const FT_Int32 loadFlags = FT_LOAD_DEFAULT) const{
			return FT_Load_Char(face, code, loadFlags);
		}

		void setSize(const unsigned w, const unsigned h) const{
			if((w && face->size->metrics.x_ppem == w) && (h && face->size->metrics.y_ppem == h))return;
			FT_Set_Pixel_Sizes(face, w, h);
		}

		void setSize(const Geom::USize2 size2) const{
			setSize( size2.x, size2.y);
		}

		[[nodiscard]] auto getHeight() const{
			return face->height;
		}

		[[nodiscard]] std::expected<FT_GlyphSlot, FT_Error> loadAndGet(const CharCode index, const FT_Int32 loadFlags = FT_LOAD_DEFAULT) const{
			if(auto error = loadChar(index, loadFlags)){
				return std::expected<FT_GlyphSlot, FT_Error>{std::unexpect, error};
			}

			return std::expected<FT_GlyphSlot, FT_Error>{std::in_place, face->glyph};
		}

		[[nodiscard]] std::string_view getFamilyName() const{
			return {face->family_name};
		}

		[[nodiscard]] std::string_view getStyleName() const{
			return {face->style_name};
		}

		[[nodiscard]] std::string getFullname() const{
			return std::format("{}.{}", getFamilyName(), getStyleName());
		}

		[[nodiscard]] auto getFaceIndex() const{
			return face->face_index;
		}

	private:
		void free();
	};

	//TODO support stroker
	struct Stroker{
		FT_Stroker stroker{};

		[[nodiscard]] Stroker(){
			FT_Stroker_New(GlobalFreeTypeLib, &stroker);
		}

		~Stroker(){
			if(stroker)FT_Stroker_Done(stroker);
		}

	};

	export struct BitmapGlyph{
		CharCode code{};
		Graphic::Pixmap bitmap{};
		FT_Glyph_Metrics metrics{};

		[[nodiscard]] BitmapGlyph() = default;

		[[nodiscard]] explicit BitmapGlyph(const CharCode code, FT_GlyphSlot glyphSlot) :
			code{code},
			bitmap{toPixmap(glyphSlot->bitmap)},
			metrics{glyphSlot->metrics}{}


		[[nodiscard]] explicit BitmapGlyph(const CharCode code, const FT_Glyph_Metrics& metrics, Geom::Vec2 scale) :
			code{code},
			metrics{metrics}{
			this->metrics.width *= scale.x;
			this->metrics.height *= scale.y;
			this->metrics.horiBearingX *= scale.x;
			this->metrics.vertBearingX *= scale.x;
			this->metrics.horiBearingY *= scale.y;
			this->metrics.vertBearingY *= scale.y;
			this->metrics.horiAdvance *= scale.x;
			this->metrics.vertAdvance *= scale.y;
		}
	};

	export struct FontFaceStorage{
		GlyphSizeType lastSize{};

		FontFace_Internal face{};

		//OPTM flat it?
		std::unordered_map<CharCode, std::unordered_map<GlyphSizeType, BitmapGlyph>> glyphs{};

		FontFaceStorage* fallback{};
		ext::TransferAdaptor<std::shared_mutex> readMutex{};

		struct EmptyFontGlyphGenerator{
			CharCode referenceCode{U' '};
			Geom::Vec2 scale{Geom::norBaseVec2<float>};
		};

		std::unordered_map<CharCode, EmptyFontGlyphGenerator> emptyFontGlyphGenerators{
				{U'\0', EmptyFontGlyphGenerator{U'\n', Geom::zeroVec2<float>}},
				{U'\n', EmptyFontGlyphGenerator{U'|', Geom::norBaseVec2<float>.copy().scl(0.5f, 1)}},
				{U' ' , EmptyFontGlyphGenerator{U'_', Geom::norBaseVec2<float>}},
				{U'\t', EmptyFontGlyphGenerator{U'\n', Geom::norBaseVec2<float>.copy().scl(4, 1)}},
			};


		[[nodiscard]] FontFaceStorage() = default;

		[[nodiscard]] explicit FontFaceStorage(const char* fontPath)
			: face{fontPath}{}

		auto& tryLoad(const CharCode code, const GlyphSizeType size){
			if(size.x == 0 && size.y == 0){
				throw std::invalid_argument("Invalid Size");
			}

			{
				std::shared_lock readLock{readMutex};
				if(const auto itr = glyphs.find(code); itr != glyphs.end()){
					if(const auto sized = itr->second.find(size); sized != itr->second.end()){
						return sized->second;
					}
				}
			}

			FT_Error error;

			{
				std::unique_lock readLock{readMutex};

				if(size != lastSize){
					face.setSize(size.x, size.y);
				}

				if(const auto itr = emptyFontGlyphGenerators.find(code); itr != emptyFontGlyphGenerators.end()){
					if(auto rst = face.loadAndGet(itr->second.referenceCode)){
						return glyphs[code].insert_or_assign(size, BitmapGlyph{code, rst.value()->metrics, itr->second.scale}).first->second;
					}else{
						error = rst.error();
					}
				}else{
					if(auto rst = face.loadAndGet(code)){
						//TODO better render process
						FT_Render_Glyph(rst.value(), FT_RENDER_MODE_NORMAL);
						return glyphs[code].insert_or_assign(size, BitmapGlyph{code, rst.value()}).first->second;
					}else{
						error = rst.error();
					}
				}
			}

			if(fallback)return fallback->tryLoad(code, size);
			//TODO failure fallback
			check(error);

			std::unreachable();
		}

		[[nodiscard]] std::string toString(const CharCode code, const GlyphSizeType size) const{
			return std::format("{}.{}.{}[{},{}]", face.getFamilyName(), face.getStyleName(), reinterpret_cast<const int&>(code), size.x, size.y);
		}

		[[nodiscard]] decltype(auto) glyphsWithName(){
			return glyphs | std::views::values | std::views::join | std::views::transform(
				[this](decltype(glyphs)::mapped_type::value_type& pair){
				return std::make_pair(toString(pair.second.code, pair.first), std::ref(pair.second));
			});
		}
	};

	export struct FontGlyphLoader{
		FontFaceStorage& storage;
		GlyphSizeType size{};
		std::vector<std::vector<CharCode>> sections{};

		void load() const{
			for (const auto code : sections | std::views::join){
				storage.tryLoad(code, size);
			}
		}
	};
}

export template <>
struct std::hash<Font::GlyphKey>{ // NOLINT(*-dcl58-cpp)
	constexpr std::size_t operator()(const Font::GlyphKey& key) const noexcept{
		return std::bit_cast<std::size_t>(key);
	}
};

BITMASK_OPS(export, Font::FontFlag);
BITMASK_OPS(export, Font::StyleFlag);

module : private;

Font::Library::Library(){
	check(FT_Init_FreeType(&library));
}

Font::Library::~Library(){
	if(library)check(FT_Done_FreeType(library));
}

Graphic::Pixmap Font::toPixmap(const FT_Bitmap& map) {
	Graphic::Pixmap pixmap{map.width, map.rows};
	auto* const data = pixmap.data();
	const auto size = map.width * map.rows;
	for(unsigned i = 0; i < size; ++i) {
		//Normal
		data[i * 4 + 0] = 0xff;
		data[i * 4 + 1] = 0xff;
		data[i * 4 + 2] = 0xff;
		data[i * 4 + 3] = map.buffer[i];
	}
	return pixmap;
}

void Font::check(FT_Error error){
	if(!error)return;

#if DEBUG_CHECK
	const char* err = FT_Error_String(error);
	std::println(std::cerr, "[Freetype] Error {}: {}", error, err);
#else
	std::println(std::cerr, "[Freetype] Error {}", error);
#endif

	throw std::runtime_error("Freetype Failed");
}

Font::FontFace_Internal::~FontFace_Internal(){
	free();
}

Font::FontFace_Internal::FontFace_Internal(const FontFace_Internal& other): face{other.face}{
	FT_Reference_Face(face);
}

Font::FontFace_Internal::FontFace_Internal(FontFace_Internal&& other) noexcept: face{other.face}{
	other.face = nullptr;
}

Font::FontFace_Internal& Font::FontFace_Internal::operator=(const FontFace_Internal& other){
	if(this == &other) return *this;
	free();
	face = other.face;
	FT_Reference_Face(face);
	return *this;
}

Font::FontFace_Internal& Font::FontFace_Internal::operator=(FontFace_Internal&& other) noexcept{
	if(this == &other) return *this;
	free();
	face = other.face;
	other.face = nullptr;
	return *this;
}

void Font::FontFace_Internal::free(){
	if(face){
		check(FT_Done_Face(face));
	}
	face = nullptr;
}

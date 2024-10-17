module;

#include <cassert>

export module Font.Manager;

export import Font;

import Graphic.ImageRegion;

import Geom.Vector2D;

import std;
import ext.heterogeneous;

export namespace Graphic{
	struct ImagePage;
	struct ImageAtlas;
}


export namespace Font{

	struct Glyph : Graphic::ImageViewRegion{
		GlyphMetrics metrics{};

		[[nodiscard]] Glyph() = default;

		[[nodiscard]] explicit Glyph(const ImageViewRegion& view, const GlyphMetrics& metrics)
			: ImageViewRegion{view}, metrics{metrics}{}

		[[nodiscard]] constexpr bool drawable() const noexcept{
			return size.area() == 0;
		}
	};

	class IndexedFontFace : FontFaceStorage{
		FontFaceID index{};

	public:
		using FontFaceStorage::FontFaceStorage;

		std::unordered_map<GlyphKey, Glyph> validGlyphs{};

		[[nodiscard]] IndexedFontFace() = default;

		[[nodiscard]] IndexedFontFace(const std::string_view fontPath, const FontFaceID index)
			: FontFaceStorage{fontPath},
			  index{index}{}

		[[nodiscard]] FontFaceID getIndex() const noexcept{
			return index;
		}

		/**
		 * @brief If guaranteed to get a glyph from cache, then atlas and page can be @code nullptr@endcode
		 */
		Glyph& obtainGlyph(GlyphKey key, const Graphic::ImageAtlas* atlas = nullptr, Graphic::ImagePage* page = nullptr);

		//make it sso optm
		[[nodiscard]] std::string toString_short(const CharCode code, const GlyphSizeType size){
			return std::format("{}.{}[{},{}]", index, reinterpret_cast<const int&>(code), size.x, size.y);
		}

		[[nodiscard]] decltype(auto) glyphsWithName_short(){
			return glyphs | std::views::values | std::views::join | std::views::transform(
				[this](decltype(glyphs)::mapped_type::reference pair){
				return std::make_pair(toString_short(pair.second.code, pair.first), std::ref(pair.second));
			});
		}
	};

	struct FontManager{
	private:
		Graphic::ImageAtlas* atlas{};
		Graphic::ImagePage* fontPage{};
		std::string fontPageName{"font"};
		ext::string_hash_map<IndexedFontFace> fontFaces{};
		std::vector<IndexedFontFace*> fontFaces_fastAccess{};

	public:
		[[nodiscard]] FontManager() = default;

		[[nodiscard]] explicit FontManager(Graphic::ImageAtlas& atlas, std::string_view fontPageName = "font");

		[[nodiscard]] std::string_view pageName() const noexcept{
			return fontPageName;
		}

		[[nodiscard]] Graphic::ImageAtlas* getAtlas() const noexcept{ return atlas; }

		[[nodiscard]] Graphic::ImagePage* getFontPage() const noexcept{ return fontPage; }

		[[nodiscard]] IndexedFontFace* getFontFace(FontFaceID id) const;

		[[nodiscard]] Glyph& getGlyph(FontFaceID faceId, GlyphKey key) const;

		[[nodiscard]] Glyph& getGlyph(std::string_view fontName, GlyphKey key);

		FontFaceID registerFace(std::string_view keyName, std::string_view fontName);

		[[nodiscard]] auto& getPrimaryFontFace() noexcept{
			assert(!fontFaces_fastAccess.empty());
			return fontFaces_fastAccess.front();
		}

		//TODO allow erase registered face?

	};
}

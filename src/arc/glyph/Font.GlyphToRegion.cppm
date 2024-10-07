//
// Created by Matrix on 2024/8/26.
//

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

	struct IndexedFontFace : FontFaceStorage{
		FontFaceID index{};

		std::unordered_map<GlyphKey, Glyph> validGlyphs{};


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
		Graphic::ImageAtlas* atlas{};
		Graphic::ImagePage* fontPage{};

		std::string fontPageName{"font"};

		ext::string_hash_map<IndexedFontFace> fontFaces{};
		std::vector<IndexedFontFace*> fontFaces_fastAccess{};

		[[nodiscard]] FontManager() = default;

		[[nodiscard]] explicit FontManager(Graphic::ImageAtlas& atlas, std::string_view fontPageName = "font");

		[[nodiscard]] IndexedFontFace* getFontFace(FontFaceID id) const;

		[[nodiscard]] Glyph& getGlyph(FontFaceID faceId, GlyphKey key) const;

		[[nodiscard]] Glyph& getGlyph(std::string_view fontName, GlyphKey key);

		FontFaceID registerFace(FontFaceStorage&& faceStorage);

		//TODO allow erase registered face?

	};
}

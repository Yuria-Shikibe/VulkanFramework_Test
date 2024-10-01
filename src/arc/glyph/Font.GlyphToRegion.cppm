//
// Created by Matrix on 2024/8/26.
//

export module Font.GlyphToRegion;

import Graphic.ImageRegion;
import Graphic.ImageAtlas;

import Geom.Vector2D;

export import Font;
import std;
import ext.heterogeneous;

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
		Glyph& obtainGlyph(const GlyphKey key, const Graphic::ImageAtlas* atlas = nullptr, Graphic::ImagePage* page = nullptr){
			if(const auto itr = validGlyphs.find(key); itr != validGlyphs.end()){
				return itr->second;
			}

			if(!atlas || !page){
				throw std::invalid_argument("Atlas or page is null");
			}

			const auto generated = tryLoad(key.code, key.size);

			Graphic::ImageViewRegion view{};

			if(generated.bitmap.valid()){
				auto allocated = atlas->allocate(*page, generated.bitmap);
				view = allocated.asView();
				page->registerNamedRegion(toString_short(key.code, key.size), std::move(allocated));
			}

			auto& rst = validGlyphs.insert_or_assign(key, Glyph{view, generated.metrics}).first->second;

			return rst;
		}

		//make it sso optm
		[[nodiscard]] std::string toString_short(const CharCode code, const GlyphSizeType size){
			return std::format("{}.{}[{},{}]", index, reinterpret_cast<const int&>(code), size.x, size.y);
		}

		[[nodiscard]] decltype(auto) glyphsWithName_short(){
			return glyphs | std::views::values | std::views::join | std::views::transform(
				[this](decltype(glyphs)::mapped_type::value_type& pair){
				return std::make_pair(toString_short(pair.second.code, pair.first), std::ref(pair.second));
			});
		}
	};

	struct FontManager{
		Graphic::ImageAtlas* atlas{};
		std::string fontPageName{"font"};
		Graphic::ImagePage* fontPage{};

		ext::string_hash_map<IndexedFontFace> fontFaces{};
		std::vector<IndexedFontFace*> fontFaces_fastAccess{};

		[[nodiscard]] FontManager() = default;

		[[nodiscard]] FontManager(Graphic::ImageAtlas& atlas, const std::string_view fontPageName = "font")
			: atlas{&atlas},
			  fontPageName{fontPageName}, fontPage{&atlas.registerPage(fontPageName)}{}

		[[nodiscard]] IndexedFontFace* getFontFace(FontFaceID id) const{
			if(id < fontFaces_fastAccess.size()){
				return fontFaces_fastAccess[id];
			}

			return nullptr;
		}

		[[nodiscard]] Glyph& getGlyph(const FontFaceID faceId, const GlyphKey key) const{
			if(fontFaces_fastAccess.size() > faceId){
				if(const auto face = fontFaces_fastAccess[faceId]){
					return face->obtainGlyph(key, atlas, fontPage);
				}
			}

			throw std::invalid_argument("Failed To Find Face with given id.");
		}

		[[nodiscard]] Glyph& getGlyph(const std::string_view name, const GlyphKey key){
			if(const auto face = fontFaces.try_find(name)){
				return face->obtainGlyph(key, atlas, fontPage);
			}

			throw std::invalid_argument("Failed To Find Face with given id.");
		}

		FontFaceID registerFace(FontFaceStorage&& faceStorage){
			auto name = faceStorage.face.getFullname();
			const auto index = fontFaces.size();
			auto [itr, suc] = fontFaces.try_emplace(std::move(name), IndexedFontFace{std::move(faceStorage)});
			if(suc){
				itr->second.index = index;
				fontFaces_fastAccess.resize(index + 1);
				fontFaces_fastAccess[index] = &itr->second;
			}

			return itr->second.index;
		}

		//TODO allow erase registered face?


		// void
	};
}

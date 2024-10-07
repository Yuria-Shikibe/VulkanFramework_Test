module Font.Manager;

import Graphic.ImageAtlas;

Font::Glyph& Font::IndexedFontFace::
obtainGlyph(const GlyphKey key, const Graphic::ImageAtlas* atlas, Graphic::ImagePage* page){
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


Font::FontManager::FontManager(Graphic::ImageAtlas& atlas, const std::string_view fontPageName): atlas{&atlas},
                                                                                                 fontPageName{fontPageName}, fontPage{&atlas.registerPage(fontPageName)}{}

Font::IndexedFontFace* Font::FontManager::getFontFace(FontFaceID id) const{
	if(id < fontFaces_fastAccess.size()){
		return fontFaces_fastAccess[id];
	}

	return nullptr;
}

Font::Glyph& Font::FontManager::getGlyph(const FontFaceID faceId, const GlyphKey key) const{
	if(fontFaces_fastAccess.size() > faceId){
		if(const auto face = fontFaces_fastAccess[faceId]){
			return face->obtainGlyph(key, atlas, fontPage);
		}
	}

	throw std::invalid_argument("Failed To Find Face with given id.");
}

Font::Glyph& Font::FontManager::getGlyph(const std::string_view fontName, const GlyphKey key){
	if(const auto face = fontFaces.try_find(fontName)){
		return face->obtainGlyph(key, atlas, fontPage);
	}

	throw std::invalid_argument("Failed To Find Face with given id.");
}

Font::FontFaceID Font::FontManager::registerFace(FontFaceStorage&& faceStorage){
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
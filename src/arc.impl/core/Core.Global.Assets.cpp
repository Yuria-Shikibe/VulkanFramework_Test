module Core.Global.Assets;

import Core.Vulkan.Manager;
import Font.Manager;
import Font.TypeSettings;

import Font;
import Graphic.Pixmap;
import Core.File;

import Assets.Directories;
import Graphic.Draw.Func;
import Graphic.ImageAtlas;

import std;

void Core::Global::Asset::init(const Vulkan::VulkanManager& manager){
	atlas = new Graphic::ImageAtlas{manager.context};

	{
		auto& imageAtlas = *atlas;

		auto& mainPage = imageAtlas.registerPage(AtlasPages::Main);
		auto& uiPage = imageAtlas.registerPage(AtlasPages::UI);
		auto& tempPage = imageAtlas.registerPage(AtlasPages::Temp);
		auto& fontPage = imageAtlas.registerPage(AtlasPages::Font);

		auto& page = fontPage.createPage(imageAtlas.context);

		page.texture.cmdClearColor(imageAtlas.obtainTransientCommand(), {1., 1., 1., 0.});

		auto pixmap = Graphic::Pixmap{::Assets::Dir::texture.find("white.png")};
		auto region = imageAtlas.allocate(mainPage, pixmap);
		region.shrink(pixmap.getWidth() / 2);
		Graphic::Draw::WhiteRegion = &mainPage.registerNamedRegion("white", std::move(region)).first;

		imageAtlas.registerNamedImageViewRegionGuaranteed("ui.base", Graphic::Pixmap{::Assets::Dir::texture.find("ui/elem-s1-back.png")});
		imageAtlas.registerNamedImageViewRegionGuaranteed("ui.edge", Graphic::Pixmap{::Assets::Dir::texture.find("ui/elem-s1-edge.png")});
		imageAtlas.registerNamedImageViewRegionGuaranteed("ui.cent", Graphic::Pixmap{::Assets::Dir::texture.find("test-1.png")});
	}

	fonts = new Font::FontManager{*atlas, AtlasPages::Font};

	{
		Font::GlobalFontManager = fonts;

		for (const auto & [name, abbr] : DefFonts){
			Font::FontFaceStorage fontStorage{Assets::Dir::font.subFile(name).absolutePath().string().c_str()};

			auto id = fonts->registerFace(abbr, Assets::Dir::font.subFile(name).absolutePath().string());
			Font::namedFonts.insert_or_assign(abbr, id);
		}
	}
}

void Core::Global::Asset::terminate(){
	delete fonts;
	delete atlas;
}

export module Core.Global.Assets;

import std;

namespace Font{
	export struct FontManager;
}


namespace Graphic{
	export struct ImageAtlas;
}

export namespace Core::Vulkan{
	class VulkanManager;
}

export namespace Core::Global::Asset{
	namespace AtlasPages{
		constexpr std::string_view Main{"main"};
		constexpr std::string_view UI{"ui"};
		constexpr std::string_view Temp{"tmp"};
		constexpr std::string_view Font{"font"};
	}

	constexpr std::array DefFonts{
		std::pair<std::string_view, std::string_view>{"SourceHanSerifSC-SemiBold.otf", "srcH"},
		std::pair<std::string_view, std::string_view>{"telegrama.otf", "tele"}
	};

	Graphic::ImageAtlas* atlas{nullptr};
	Font::FontManager* fonts{nullptr};

	void init(const Vulkan::VulkanManager& manager);

	void terminate();
}

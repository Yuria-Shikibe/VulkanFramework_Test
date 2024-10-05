module;

#include "../src/spec.hpp"

export module Core.Global.UI;

export namespace Core::Vulkan{
	class VulkanManager;
}

export namespace Graphic{
	struct RendererUI;
}

export namespace Core::UI{
	 SPEC_WEAK struct LooseGroup;
	 SPEC_WEAK struct Element;
	 SPEC_WEAK struct Root;
	 SPEC_WEAK struct Scene;
}

namespace Core::UI{
	Graphic::RendererUI* renderer{};
	Core::UI::Root* root;

	void init(Vulkan::VulkanManager& manager);

	Core::UI::LooseGroup& getMainGroup();

	void terminate();

}

export namespace Core::Global::UI{
	using ::Core::UI::root;
	using ::Core::UI::renderer;
	using ::Core::UI::init;
	using ::Core::UI::getMainGroup;
	using ::Core::UI::terminate;
}

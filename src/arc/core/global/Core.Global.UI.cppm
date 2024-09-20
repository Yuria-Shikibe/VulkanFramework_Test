export module Core.Global.UI;

export namespace Core::Vulkan{
	class VulkanManager;
}


export namespace Graphic{
	struct RendererUI;
}

export namespace Core::UI{
	struct BedFace;
	struct Root;
}

export namespace Core::Global::UI{
	Graphic::RendererUI* renderer{};
	Core::UI::Root* root;

	void init(Vulkan::VulkanManager& manager);

	Core::UI::BedFace& getMainGroup();

	void terminate();
}

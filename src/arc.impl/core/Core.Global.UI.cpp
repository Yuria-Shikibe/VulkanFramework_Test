module Core.Global.UI;

import Core.UI.Root;
import Core.UI.BedFace;
import Core.UI.Element;
import Core.Vulkan.Manager;
import Graphic.Renderer.UI;

void Core::Global::UI::init(Vulkan::VulkanManager& manager){
	renderer = new Graphic::RendererUI{manager.context};
	root = new Core::UI::Root{Core::UI::Scene{new Core::UI::BedFace, renderer}};

	
	manager.registerResizeCallback(
		[&](auto& e){
			if(e.area()){
				renderer->resize(static_cast<Geom::USize2>(e));
				manager.uiPort = renderer->getPort();
			}
		}, [&](auto e){
			renderer->init(e);
			manager.uiPort = renderer->getPort();
		});

	manager.registerResizeCallback(
		[&](auto& e){
			if(e.area()){
				root->resize(e.template as<float>());
			}
		}, [&](auto e){
			root->resize(e.template as<float>());
		});
}

Core::UI::BedFace& Core::Global::UI::getMainGroup(){
	return root->rootOf<Core::UI::BedFace>(Core::UI::SceneName::Main);
}

void Core::Global::UI::terminate(){
	delete root;
	delete renderer;
}

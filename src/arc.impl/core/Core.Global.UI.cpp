module Core.Global.UI;

import Graphic.Renderer.UI;
import Core.UI.Root;
import Core.UI.LooseGroup;
import Core.UI.Element;
import Core.Vulkan.Manager;

void Core::UI::init(Vulkan::VulkanManager& manager){
	renderer = new Graphic::RendererUI{manager.context};
	root = new Root{Scene{SceneName::Main, new LooseGroup, renderer}};

	
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

Core::UI::LooseGroup& Core::UI::getMainGroup(){
	return root->rootOf<Core::UI::LooseGroup>(Core::UI::SceneName::Main);
}

void Core::UI::terminate(){
	delete root;
	delete renderer;
}

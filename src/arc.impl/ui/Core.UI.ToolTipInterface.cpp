module Core.UI.ToolTipInterface;

import Core.UI.Scene;

Core::UI::Group* Core::UI::ToolTipOwner::getToolTipTrueParent(Scene& scene) const{
	if(auto p = specifiedParent()){
		return p;
	}

	return scene.root;
}

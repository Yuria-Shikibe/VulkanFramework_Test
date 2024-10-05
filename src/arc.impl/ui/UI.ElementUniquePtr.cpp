module Core.UI.ElementUniquePtr;

import Core.UI.Element;

Core::UI::ElementUniquePtr::~ElementUniquePtr(){
	delete element;
}

void Core::UI::ElementUniquePtr::setGroupAndScene(Group* group, Scene* scene) const{
	element->setParent(group);
	element->setScene(scene);
}

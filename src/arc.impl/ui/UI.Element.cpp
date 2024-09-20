module Core.UI.Element;

import Graphic.Renderer.UI;
import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;
import Core.Vulkan.Vertex;

void Core::UI::DefElementDrawer::draw(const Element& element) const {
	using namespace Graphic;

	Draw::DrawContext context{};
	BatchAutoParam<Vulkan::Vertex_UI> ac{element.getScene()->renderer->batch, context.whiteRegion};

	auto param = ac.get();

	context.color = element.getCursorState().focused ? Colors::WHITE : Colors::YELLOW;
	context.stroke = 2.f;


	if(element.getCursorState().pressed){
		context.color.a = 0.3f;
		Draw::Drawer<Vulkan::Vertex_UI>::rectOrtho(++param, element.prop().getValidBound_absolute(), context.color);
	}
	context.color.a = 1.f;
	Draw::Drawer<Vulkan::Vertex_UI>::Line::rectOrtho(param, context.stroke, element.prop().getValidBound_absolute(), context.color);

}

void Core::UI::Element::notifyRemove(){
	clearExternalReferences();
	parent->postRemove(this);
}

void Core::UI::Element::clearExternalReferences() noexcept{
	if(scene){
		scene->dropAllFocus(this);
		scene = nullptr;
	}
}

bool Core::UI::Element::isFocusedScroll() const noexcept{
	return scene && scene->currentScrollFocus == this;
}

void Core::UI::Element::setFocusedScroll(const bool focus) noexcept{
	if(!focus && !isFocusedScroll())return;
	this->scene->currentScrollFocus = focus ? this : nullptr;
}

bool Core::UI::Element::containsPos(const Geom::Vec2 absPos) const noexcept{
	return (!parent || parent->containsPos_parent(absPos)) && property.getValidBound_absolute().containsPos_edgeExclusive(absPos);
}

void Core::UI::Element::notifyLayoutChanged(const SpreadDirection toDirection){
	if(toDirection & SpreadDirection::local)layoutState.setSelfChanged();
	if(toDirection & SpreadDirection::super && parent){
		if(parent->layoutState.notifyFromChildren()){
			parent->notifyLayoutChanged(toDirection - SpreadDirection::child);
		}
	}
	if(toDirection & SpreadDirection::child){
		for (auto && element : getChildren()){
			if(element->layoutState.notifyFromParent()){
				element->notifyLayoutChanged(toDirection - SpreadDirection::super);
			}
		}
	}
}


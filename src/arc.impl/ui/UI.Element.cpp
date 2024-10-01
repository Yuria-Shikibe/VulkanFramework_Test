module Core.UI.Element;

import Graphic.Renderer.UI;
import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;
import Core.Vulkan.Vertex;



void Core::UI::DefElementDrawer::draw(const Element& element) const {
	using namespace Graphic;

	InstantBatchAutoParam param{static_cast<RendererUI*>(element.getScene()->renderer)->batch, Draw::WhiteRegion};
	auto color = element.getCursorState().focused ? Colors::WHITE : Colors::YELLOW;

	if(element.getCursorState().pressed){
		color.a = 0.3f;
		Draw::Drawer<Vulkan::Vertex_UI>::rectOrtho(++param, element.prop().getValidBound_absolute(), color);
	}

	color.a = 1.f;
	Draw::Drawer<Vulkan::Vertex_UI>::Line::rectOrtho(param, 2.f, element.prop().getValidBound_absolute(), color);

}

Graphic::Batch_Exclusive& Core::UI::Element::getBatch() const noexcept(true){
#if DEBUG_CHECK
	if(!getScene())throw std::logic_error("nullptr scene");
#endif
	return getScene()->renderer->batch;
}

void Core::UI::Element::registerAsyncTask(){

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

	if(parent){
		if(toDirection & SpreadDirection::super || toDirection & SpreadDirection::child_item){
			if(parent->layoutState.tryNotifyFromChildren() || toDirection & SpreadDirection::child_item){
				parent->notifyLayoutChanged(toDirection - SpreadDirection::child);
			}
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


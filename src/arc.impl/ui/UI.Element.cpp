module;

#include <cassert>

module Core.UI.Element;


import Graphic.Renderer.UI;

import Core.UI.Group;
import Core.UI.Scene;
import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;

import Core.Vulkan.Vertex;


void Core::UI::DefElementDrawer::draw(const Element& element) const {
	using namespace Graphic;

	InstantBatchAutoParam param{element.getBatch(), Draw::WhiteRegion};
	auto color = element.getCursorState().focused ? Colors::YELLOW : element.graphicProp().baseColor;

	if(element.getCursorState().pressed){
		color.a = 0.3f * element.graphicProp().getOpacity();
		Draw::Drawer<Vulkan::Vertex_UI>::rectOrtho(++param, element.prop().getValidBound_absolute(), color);
	}

	color.a = 1.f * element.graphicProp().getOpacity();
	Draw::Drawer<Vulkan::Vertex_UI>::Line::rectOrtho(param, 2.f, element.prop().getValidBound_absolute(), color);

}

Graphic::Batch_Exclusive& Core::UI::Element::getBatch() const noexcept(true){
#if DEBUG_CHECK
	if(!getScene())throw std::logic_error("nullptr scene");
#endif
	return getScene()->renderer->batch;
}

Graphic::RendererUI& Core::UI::Element::getRenderer() const noexcept{
	return *getScene()->renderer;
}

void Core::UI::Element::updateOpacity(const float val){
	if(Util::tryModify(property.graphicData.inherentOpacity, val)){
		for (const auto& element : getChildren()){
			element->updateOpacity(graphicProp().getOpacity());
		}
	}
}

void Core::UI::Element::removeSelfFromParent(){
	assert(!isRootElement());

	parent->postRemove(this);
}

void Core::UI::Element::removeSelfFromParent_instantly(){
	assert(!isRootElement());

	parent->instantRemove(this);
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

bool Core::UI::Element::isFocused() const noexcept{
	return scene && scene->currentCursorFocus == this;
}

bool Core::UI::Element::isInbounded() const noexcept{
	return scene && std::ranges::contains(scene->lastInbounds, this);
}

void Core::UI::Element::setFocusedScroll(const bool focus) noexcept{
	if(!focus && !isFocusedScroll())return;
	this->scene->currentScrollFocus = focus ? this : nullptr;
}

bool Core::UI::Element::containsPos(const Geom::Vec2 absPos) const noexcept{
	return containsPos_parent(absPos) && containsPos_self(absPos);
}

bool Core::UI::Element::containsPos_self(const Geom::Vec2 absPos, const float margin) const noexcept{
	return property.getValidBound_absolute().expand(margin, margin).containsPos_edgeExclusive(absPos);
}

void Core::UI::Element::buildTooltip() noexcept{
	getScene()->tooltipManager.appendToolTip(*this);
}

bool Core::UI::Element::containsPos_parent(const Geom::Vec2 cursorPos) const{
	return (!parent || parent->containsPos_parent(cursorPos));
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

void Core::UI::Element::update(const float delta_in_ticks){
	cursorState.update(delta_in_ticks);

	if(cursorState.focused){
		getScene()->tooltipManager.tryAppendToolTip(*this);
	}

	for(float actionDelta = delta_in_ticks; !actions.empty(); ){
		const auto& current = actions.front();

		actionDelta = current->update(actionDelta, *this);

		if(actionDelta >= 0) [[unlikely]] {
			actions.pop();
		}else{
			break;
		}
	}
}

void Core::UI::Element::drawMain() const{
	property.graphicData.drawer->draw(*this);
}

void Core::UI::Element::dropToolTipIfMoved() const{
	if(tooltipProp.useStagnateTime)getScene()->tooltipManager.requestDrop(*this);
}

std::vector<Core::UI::Element*> Core::UI::Element::dfsFindDeepestElement(Geom::Vec2 cursorPos){
	std::vector<Element*> rst{};

	iterateAll_DFSImpl(cursorPos, rst, this);

	return rst;
}

void Core::UI::iterateAll_DFSImpl(Geom::Vec2 cursorPos, std::vector<Element*>& selected, Element* current){
	if(current->isInteractable() && current->containsPos(cursorPos)){
		selected.push_back(current);
	}

	if(current->touchDisabled() || !current->hasChildren()) return;

	for(const auto& child : current->getChildren() | std::views::reverse){
		iterateAll_DFSImpl(cursorPos, selected, child.get());
	}
}



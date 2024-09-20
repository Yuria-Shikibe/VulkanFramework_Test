module Core.UI.Element;

import Core.Ctrl.KeyPack;
import Graphic.Renderer.UI;
import Core.UI.Event;
import ext.concepts;



void iterateAll_DFSImpl(const Core::UI::Scene& scene, std::vector<Core::UI::Element*>& selected, Core::UI::Element* current){
	if(current->isInteractable() && current->containsPos(scene.cursorPos)){
		selected.push_back(current);
	}

	if(current->touchDisabled() || !current->hasChildren())return;

	for(const auto& child : current->getChildren() | std::views::reverse){
		iterateAll_DFSImpl(scene, selected, child.get());
	}
}

std::vector<Core::UI::Element*> Core::UI::Scene::dfsFindDeepestElement(Element* target) const{
	std::vector<Core::UI::Element*> rst{};

	iterateAll_DFSImpl(*this, rst, target);

	return rst;
}

Core::UI::Scene::Scene(const ext::owner<Group*> root, Graphic::RendererUI* renderer, Bundle* bundle) :
	SceneBase{root, renderer, bundle}{

	root->setScene(this);
	root->interactivity = Interactivity::childrenOnly;
}

Core::UI::Scene::~Scene(){
	delete root.handle;
}

void Core::UI::Scene::dropAllFocus(const Element* target){
	if(currentCursorFocus == target)currentCursorFocus = nullptr;
	if(currentScrollFocus == target)currentScrollFocus = nullptr;
}



void Core::UI::Scene::trySwapFocus(Element* newFocus){



	if(newFocus == currentCursorFocus)return;

	if(currentCursorFocus){
		if(currentCursorFocus->maintainFocusByMouse()){
			if(!isMousePressed()){
				swapFocus(newFocus);
			}else return;
		}else{
			swapFocus(newFocus);
		}
	}else{
		swapFocus(newFocus);
	}
}

void Core::UI::Scene::swapFocus(Element* newFocus){
	if(currentCursorFocus){
		for(auto& state : mouseKeyStates){
			state.clear(cursorPos);
		}
		currentCursorFocus->events().fire(Event::EndFocus{cursorPos});
	}

	currentCursorFocus = newFocus;

	if(currentCursorFocus){
		currentCursorFocus->events().fire(Event::BeginFocus{cursorPos});
	}
}

void Core::UI::Scene::onMouseAction(const int key, const int action, const int mode){
	if(currentCursorFocus)currentCursorFocus->events().fire(
			Event::Click{cursorPos, Ctrl::KeyPack{key, action, mode}}
		);

	if(action == Ctrl::Act::Press){
		mouseKeyStates[key].reset(cursorPos);
	}

	if(action == Ctrl::Act::Release){
		mouseKeyStates[key].clear(cursorPos);
	}

	if(currentCursorFocus && currentCursorFocus->maintainFocusByMouse()){
		onCursorPosUpdate(cursorPos);
	}
}

void Core::UI::Scene::onKeyAction(const int key, const int action, const int mode) const{
	if(currentCursorFocus){
		currentCursorFocus->inputKey(key, action, mode);
	}
}

void Core::UI::Scene::onTextInput(int code, int mode){

}

void Core::UI::Scene::onScroll(const Geom::Vec2 scroll) const{
	if(currentScrollFocus)currentScrollFocus->events().fire(Event::Scroll{scroll});
}

void Core::UI::Scene::onCursorPosUpdate(const Geom::Vec2 newPos){
	cursorPos = newPos;

	auto elem = dfsFindDeepestElement(root);
	trySwapFocus(elem.empty() ? nullptr : elem.back());
	updateInbounds(std::move(elem));

	if(!currentCursorFocus)return;

	Event::Drag dragEvent{};
	for (const auto& [i, state] : mouseKeyStates | std::views::enumerate){
		if(!state.pressed)continue;

		dragEvent = {state.src, newPos, {static_cast<int>(i), 0, 0}};
		currentCursorFocus->events().fire(dragEvent);
	}
}

void Core::UI::Scene::resize(const Geom::Vec2 size){
	if(this->size == size)return;

	this->size = size;
	root->resize(size);
}

void Core::UI::Scene::update(const float delta_in_ticks) const{
	root->update(delta_in_ticks);
}

void Core::UI::Scene::layout() const{
	root->tryLayout();
}

void Core::UI::Scene::draw() const{
	root->drawChildren(renderer->getCurrentScissor());
}

Core::UI::Scene::Scene(Scene&& other) noexcept: SceneBase{std::move(other)}{
	root->setScene(this);
}

Core::UI::Scene& Core::UI::Scene::operator=(Scene&& other) noexcept{
	if(this == &other) return *this;
	SceneBase::operator =(std::move(other));
	root->setScene(this);
	return *this;
}

void Core::UI::Scene::updateInbounds(std::vector<Element*>&& next){
	auto [i1, i2] = std::ranges::mismatch(lastInbounds, next);

	for (const auto& element : std::ranges::subrange{i1, lastInbounds.end()}){
		element->events().fire(Event::Exbound{cursorPos});
	}

	for (const auto& element : std::ranges::subrange{i2, next.end()}){
		element->events().fire(Event::Inbound{cursorPos});
	}

	lastInbounds = std::move(next);
}

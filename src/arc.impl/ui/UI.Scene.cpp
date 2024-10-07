module;

module Core.UI.Scene;

import Core.UI.Group;
import Core.UI.Element;
import Graphic.Renderer.UI;
import Core.Ctrl.KeyPack;
import Core.UI.Event;
import ext.concepts;
import ext.algo;

void Core::UI::Scene::joinTasks(){
	for(auto&& asyncTaskOwner : asyncTaskOwners){
		asyncTaskOwner->getFuture();
	}

	asyncTaskOwners.clear();
}

Core::UI::Scene::Scene(const ext::owner<Group*> root, Graphic::RendererUI* renderer, Bundle* bundle) :
	SceneBase{root, renderer, bundle}{
	root->setScene(this);
	root->interactivity = Interactivity::childrenOnly;
}

Core::UI::Scene::~Scene(){
	delete root.handle;
}

void Core::UI::Scene::registerAsyncTaskElement(Element* element){
	// if constexpr (DEBUG_CHECK){
	// 	if(std::ranges::contains(asyncTaskOwners, element)){
	// 		throw std::invalid_argument("duplicated async task owner");
	// 	}
	// }
	asyncTaskOwners.insert(element);
}

void Core::UI::Scene::registerIndependentLayout(Element* element){
	// if constexpr (DEBUG_CHECK){
	// 	if(std::ranges::contains(independentLayout, element)){
	// 		throw std::invalid_argument("duplicated async task owner");
	// 	}
	// }
	independentLayout.insert(element);
}

void Core::UI::Scene::dropAllFocus(const Element* target){
	if(currentCursorFocus == target) currentCursorFocus = nullptr;
	if(currentScrollFocus == target) currentScrollFocus = nullptr;
	std::erase(lastInbounds, target);
	asyncTaskOwners.erase(const_cast<Element*>(target));
	independentLayout.erase(const_cast<Element*>(target));
	tooltipManager.requestDrop(*target);
}

void Core::UI::Scene::trySwapFocus(Element* newFocus){
	if(newFocus == currentCursorFocus) return;

	if(currentCursorFocus){
		if(currentCursorFocus->maintainFocusByMouse()){
			if(!isMousePressed()){
				swapFocus(newFocus);
			} else return;
		} else{
			swapFocus(newFocus);
		}
	} else{
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
	if(currentCursorFocus)
		currentCursorFocus->events().fire(
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

void Core::UI::Scene::onTextInput(int code, int mode){}

void Core::UI::Scene::onScroll(const Geom::Vec2 scroll) const{
	if(currentScrollFocus) currentScrollFocus->events().fire(Event::Scroll{scroll});
}

void Core::UI::Scene::onCursorPosUpdate(const Geom::Vec2 newPos){
	const auto delta = newPos - cursorPos;
	cursorPos = newPos;

	std::vector<Element*> inbounds{};

	for (auto && activeTooltip : tooltipManager.getActiveTooltips() | std::views::reverse){
		inbounds = activeTooltip.element->dfsFindDeepestElement(cursorPos);
		if(!inbounds.empty())break;
	}

	if(inbounds.empty()){
		inbounds = root->dfsFindDeepestElement(cursorPos);
	}

	updateInbounds(std::move(inbounds));

	if(!currentCursorFocus) return;

	Event::Drag dragEvent{};
	for(const auto& [i, state] : mouseKeyStates | std::views::enumerate){
		if(!state.pressed) continue;

		dragEvent = {state.src, newPos, {static_cast<int>(i), 0, 0}};
		currentCursorFocus->events().fire(dragEvent);
	}

	currentCursorFocus->events().fire(Event::Moved{delta});
}

void Core::UI::Scene::resize(const Geom::Vec2 size){
	if(this->size == size) return;

	this->size = size;
	root->resize(size);
	tooltipManager.clear();
}

void Core::UI::Scene::update(const float delta_in_ticks){
	tooltipManager.update(delta_in_ticks);
	root->update(delta_in_ticks);
}

void Core::UI::Scene::layout(){
	root->tryLayout();

	for(const auto layout : independentLayout){
		layout->tryLayout();
	}
	independentLayout.clear();
}

void Core::UI::Scene::draw() const{
	root->drawChildren(renderer->getCurrentScissor());
	tooltipManager.draw();
}

Core::UI::Scene::Scene(Scene&& other) noexcept: SceneBase{std::move(other)},
                                                tooltipManager{std::move(other.tooltipManager)}{
	root->setScene(this);
	tooltipManager.scene = this;
}

Core::UI::Scene& Core::UI::Scene::operator=(Scene&& other) noexcept{
	if(this == &other) return *this;
	SceneBase::operator =(std::move(other));
	tooltipManager = std::move(other.tooltipManager);
	tooltipManager.scene = this;

	root->setScene(this);
	return *this;
}

void Core::UI::Scene::updateInbounds(std::vector<Element*>&& next){
	auto [i1, i2] = std::ranges::mismatch(lastInbounds, next);

	for(const auto& element : std::ranges::subrange{i1, lastInbounds.end()}){
		element->events().fire(Event::Exbound{cursorPos});
	}

	for(const auto& element : std::ranges::subrange{i2, next.end()}){
		element->events().fire(Event::Inbound{cursorPos});
	}

	lastInbounds = std::move(next);

	trySwapFocus(lastInbounds.empty() ? nullptr : lastInbounds.back());
}
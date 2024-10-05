module;

#include <cassert>

module Core.UI.ToolTipManager;

import Core.UI.Group;
import Core.UI.Element;
import Core.UI.Scene;
import Align;

namespace Core::UI{
	void ToolTipManager::ValidToolTip::updatePosition(const ToolTipManager& manager){
		assert(owner != nullptr);

		// constexpr Align::Spacing spacing{0, 0, 0, 0};
		Geom::Vec2 followOffset{};

		const auto [pos, align] = owner->getAlignPolicy();

		const auto elemOffset = Align::getOffsetOf(align, element->getSize())/* + Align::getOffsetOf(align, spacing)*/;

		std::visit([&] <typename T> (const T& val){
			if constexpr (std::same_as<T, Geom::Vec2>){
				followOffset = val;
			} else if constexpr(std::same_as<T, TooltipFollow>){
				switch(val){
					case TooltipFollow::cursor :{
						followOffset = manager.getCursorPos();
						break;
					}

					case TooltipFollow::none :{
						if(!isPosSet()){
							followOffset = manager.getCursorPos();
						}else{
							followOffset = lastPos;
						}

						break;
					}

					default: std::unreachable();
				}
			}
		}, pos);

		followOffset += elemOffset;
		lastPos = followOffset;

		element->updateAbsSrc(followOffset);

		// table->calAbsoluteSrc(nullptr);
	}

	Geom::Vec2 ToolTipManager::getCursorPos() const noexcept{
		return scene->cursorPos;
	}

	ToolTipManager::ValidToolTip* ToolTipManager::appendToolTip(TooltipOwner& owner){
		auto rst = owner.build(*scene);
		auto& val = actives.emplace_back(std::move(rst), &owner);
		val.element->layout();
		val.updatePosition(*this);
		return &val;
	}

	void ToolTipManager::update(const float delta_in_time){
		updateDropped(delta_in_time);

		for (auto&& active : actives){
			active.element->update(delta_in_time);
			active.element->tryLayout();
			active.updatePosition(*this);
		}

		if(!scene->isMousePressed()){
			auto lastNotInBound = std::ranges::find_if_not(actives, [this](const ValidToolTip& toolTip){
			   return toolTip.element->containsPos_self(getCursorPos(), 15.f) || !toolTip.owner->tooltipShouldDrop(getCursorPos());
		   });

			dropFrom(lastNotInBound);
		}
	}

	void ToolTipManager::draw() const{
		auto bound = scene->getBound();
		for (auto&& elem : dropped | std::views::reverse){
			elem.element->tryDraw(bound);
		}

		for (auto&& elem : actives){
			elem.element->tryDraw(bound);
		}
	}

	// std::vector<Element*> ToolTipManager::getInbounded(){
	//
	// }

	bool ToolTipManager::drop(const ActivesItr be, const ActivesItr se){
		if(be == se)return false;

		auto range = std::ranges::subrange{be, se};
		for (auto && validToolTip : range){
			validToolTip.owner->notifyDrop();
		}

		dropped.append_range(range | std::ranges::views::as_rvalue | std::views::transform([](ValidToolTip&& validToolTip){
			return DroppedToolTip{std::move(validToolTip).release(), 15.f};
		}));

		actives.erase(be, se);
		return true;
	}

	void ToolTipManager::updateDropped(const float delta_in_time){
		std::erase_if(dropped, [&](decltype(dropped)::reference dropped){
			dropped.element->update(delta_in_time);
			dropped.remainTime -= delta_in_time;
			return dropped.remainTime <= 0;
		});
	}
}

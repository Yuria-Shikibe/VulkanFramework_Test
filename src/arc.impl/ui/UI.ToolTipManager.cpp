module;

#include <cassert>

module Core.UI.ToolTipManager;

import Core.UI.Group;
import Core.UI.Element;
import Core.UI.Action.Graphic;
import Core.UI.Scene;
import Align;

namespace Core::UI{
	ToolTipManager::DroppedToolTip::DroppedToolTip(ElementUniquePtr&& element, float remainTime): element{std::move(element)},
		remainTime{remainTime}{
		this->element->pushAction(Actions::AlphaAction{RemoveFadeTime, 0.0f});
	}

	void ToolTipManager::ValidToolTip::updatePosition(const ToolTipManager& manager){
		assert(owner != nullptr);

		// constexpr Align::Spacing spacing{0, 0, 0, 0};

		const auto [pos, align] = owner->getAlignPolicy();

		const auto elemOffset = Align::getOffsetOf(align, element->getSize())/* + Align::getOffsetOf(align, spacing)*/;
		Geom::Vec2 followOffset{elemOffset};

		std::visit([&] <typename T> (const T& val){
			if constexpr (std::same_as<T, Geom::Vec2>){
				followOffset += val;
			} else if constexpr(std::same_as<T, TooltipFollow>){
				switch(val){
					case TooltipFollow::cursor :{
						followOffset += manager.getCursorPos();
						break;
					}

					case TooltipFollow::initial :{
						if(!isPosSet()){
							followOffset += manager.getCursorPos();
						}else{
							followOffset = lastPos;
						}

						break;
					}

					default: std::unreachable();
				}
			}
		}, pos);

		if(followOffset.equalsTo(lastPos))return;
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
		val.element->updateOpacity(0.f);
		val.element->pushAction(Actions::AlphaAction{3.0f, 1.0f});
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
			const auto lastNotInBound = std::ranges::find_if_not(actives, [this](const ValidToolTip& toolTip){
				const auto target = std::visit([] <typename T>(const T& val){
					if constexpr(std::same_as<T, Geom::Vec2>){
						return TooltipFollow::depends;
					} else if constexpr(std::same_as<T, TooltipFollow>){
						return val;
					}
				}, toolTip.owner->getAlignPolicy().pos);


				const bool selfContains = (target != TooltipFollow::cursor && toolTip.element->containsPos_self(getCursorPos(), 15.f));
				const bool ownerContains = (target != TooltipFollow::initial && !toolTip.owner->tooltipShouldDrop(getCursorPos()));
				return selfContains || ownerContains;
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

	bool ToolTipManager::drop(const ActivesItr be, const ActivesItr se){
		if(be == se)return false;

		auto range = std::ranges::subrange{be, se};
		for (auto && validToolTip : range){
			validToolTip.owner->notifyDrop();
		}

		dropped.append_range(range | std::ranges::views::as_rvalue | std::views::transform([](ValidToolTip&& validToolTip){
			return DroppedToolTip{std::move(validToolTip).release(), RemoveFadeTime};
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

module;

#include <cassert>

module Core.UI.ToolTipManager;

import Core.UI.Element;
import Core.UI.Scene;
import Align;

namespace Core::UI{
	void ToolTipManager::ValidToolTip::updatePosition(const ToolTipManager& manager){
		assert(owner != nullptr);

		constexpr Align::Spacing spacing{6, 6, 6, 6};
		Geom::Vec2 followOffset{};

		const auto [pos, align] = owner->alignPolicy();

		const auto elemOffset = Align::getOffsetOf(align, element->getSize()) + Align::getOffsetOf(align, spacing);

		std::visit([&] <typename T> (const T& val){
			if constexpr (std::same_as<T, Geom::Vec2>){
				followOffset = val;
			} else if constexpr(std::same_as<T, ToolTipFollow>){
				switch(val){
					case ToolTipFollow::cursor :{
						followOffset = manager.getCursorPos();
						break;
					}

					case ToolTipFollow::none :{
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

	ToolTipManager::ValidToolTip* ToolTipManager::appendToolTip(ToolTipOwner& owner){
		auto rst = owner.build(*scene);
		return &actives.emplace_back(std::move(rst), &owner);
	}

	void ToolTipManager::update(const float delta_in_time){
		updateDropped(delta_in_time);

		for (auto&& active : actives){
			active.updatePosition(*this);
		}
	}

	void ToolTipManager::updateDropped(const float delta_in_time){
		std::erase_if(dropped, [&](decltype(dropped)::reference dropped){
			dropped.element->update(delta_in_time);
			dropped.remainTime -= delta_in_time;
			return dropped.remainTime <= 0;
		});
	}
}

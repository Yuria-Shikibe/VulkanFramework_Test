//
// Created by Matrix on 2024/10/1.
//

export module Core.UI.ToolTipManager;

export import Core.UI.ElementUniquePtr;
export import Core.UI.ToolTipInterface;
import Geom.Vector2D;

import std;

export namespace Core::UI{
	// struct Scene;

	class ToolTipManager{
		static constexpr float RemoveFadeTime = 15.f;
		struct DroppedToolTip{
			ElementUniquePtr element{};
			float remainTime{};

			[[nodiscard]] DroppedToolTip(ElementUniquePtr&& element, float remainTime);
		};

		struct ValidToolTip{
			ElementUniquePtr element{};

			TooltipOwner* owner{};
			Geom::Vec2 lastPos{Geom::SNAN2};

			[[nodiscard]] bool isPosSet() const noexcept{
				return !lastPos.isNaN();
			}

			void resetLastPos(){
				lastPos = Geom::SNAN2;
			}

			void updatePosition(const ToolTipManager& manager);

			decltype(auto) release() && {
				return std::move(element);
			}
		};

		std::vector<DroppedToolTip> dropped{};
		std::vector<ValidToolTip> actives{};

		using ActivesItr = decltype(actives)::iterator;

	public:
		Scene* scene{};

		[[nodiscard]] ToolTipManager() = default;

		bool hasInstance(TooltipOwner& owner){
			return std::ranges::contains(actives | std::views::reverse | std::views::transform(&ValidToolTip::owner), &owner);
		}

		[[nodiscard]] TooltipOwner* getTopFocus() const noexcept{
			return actives.empty() ? nullptr : actives.back().owner;
		}

		[[nodiscard]] Geom::Vec2 getCursorPos() const noexcept;

		ValidToolTip* appendToolTip(TooltipOwner& owner);

		ValidToolTip* tryAppendToolTip(TooltipOwner& owner){
			if(owner.tooltipShouldBuild(getCursorPos()) && !hasInstance(owner)){
				return appendToolTip(owner);
			}

			return nullptr;
		}

		void update(float delta_in_time);

		bool requestDrop(const TooltipOwner& owner){
			const auto be = std::ranges::find(actives, &owner, &ValidToolTip::owner);

			return dropFrom(be);
		}

		void draw() const;

		auto& getActiveTooltips() noexcept{
			return actives;
		}

		void clear() noexcept{
			dropAll();
			dropped.clear();
		}

	private:
		bool drop(ActivesItr be, ActivesItr se);

		bool dropOne(const ActivesItr where){
			return drop(where, std::next(where));
		}

		bool dropAll(){
			return drop(actives.begin(), actives.end());
		}

		bool dropFrom(const ActivesItr where){
			return drop(where, actives.end());
		}

		void updateDropped(float delta_in_time);
	};
}

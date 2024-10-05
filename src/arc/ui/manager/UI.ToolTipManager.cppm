//
// Created by Matrix on 2024/10/1.
//

export module Core.UI.ToolTipManager;

import std;
import Core.UI.ElementUniquePtr;
import Core.UI.ToolTipInterface;
import Geom.Vector2D;

export namespace Core::UI{
	// struct Scene;

	class ToolTipManager{
		struct DroppedToolTip{
			ElementUniquePtr element{};
			float remainTime{};
		};

		struct ValidToolTip{
			ElementUniquePtr element{};

			ToolTipOwner* owner{};
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

		ElementUniquePtr root{};

		Scene* scene{};

		using ActivesItr = decltype(actives)::iterator;

	public:
		[[nodiscard]] ToolTipManager() = default;

		bool hasInstance(ToolTipOwner* owner){
			return std::ranges::contains(actives | std::views::transform(&ValidToolTip::owner), owner);
		}

		[[nodiscard]] ToolTipOwner* getTopFocus() const noexcept{
			return actives.empty() ? nullptr : actives.back().owner;
		}

		[[nodiscard]] Geom::Vec2 getCursorPos() const noexcept;

		ValidToolTip* appendToolTip(ToolTipOwner& owner);

		ValidToolTip* tryAppendToolTip(ToolTipOwner& owner){
			if(owner.shouldBuild(getCursorPos())){
				return appendToolTip(owner);
			}

			return nullptr;
		}

		void update(float delta_in_time);

		void eraseToolTipByOwner(ToolTipOwner& owner){
			const auto be = std::ranges::find(actives, &owner, &ValidToolTip::owner);
			dropFrom(be);
		}

	private:
		void drop(const ActivesItr be, const ActivesItr se){
			auto range = std::ranges::subrange{be, se};
			for (auto && validToolTip : range){
				validToolTip.owner->notifyDrop();
			}

			dropped.append_range(range | std::ranges::views::as_rvalue | std::views::transform([](ValidToolTip&& validToolTip){
				return DroppedToolTip{std::move(validToolTip).release(), 15.f};
			}));

			actives.erase(be, se);
		}

		void dropOne(const ActivesItr where){
			drop(where, std::next(where));
		}

		void dropAll(){
			drop(actives.begin(), actives.end());
		}

		void dropFrom(const ActivesItr where){
			drop(where, actives.end());
		}

		void updateDropped(float delta_in_time);
	};
}

module;

#include <cassert>

export module Game.Delay;

import Math.Timed;
import Core.Unit;

import ext.concepts;
import std;
import ext.algo;

export namespace Game {
	// using TickRatio = std::ratio<1, 60>;

	enum struct ActionPriority{
		unignorable,
		important,
		normal,
		minor,

		last
	};

	struct ActionParam{
		Core::Tick delay{};
		unsigned repeat{};
		bool noSuspend{};
	};

	struct DelayAction {
	private:
		Math::Timed progress{};
		unsigned repeatedCount{};

	public:
		unsigned repeatCount{};
		std::move_only_function<void() const> action{};

		[[nodiscard]] DelayAction() = default;


		template <std::invocable Fn>
		[[nodiscard]] DelayAction(const Core::Tick tick, Fn&& func)
			: progress(tick.count()), action{std::forward<Fn>(func)} {
		}

		template <std::invocable Fn>
		[[nodiscard]] DelayAction(const Core::Tick tick, const unsigned repeat, Fn&& func)
			: progress(tick.count()), repeatCount{repeat}, action{std::forward<Fn>(func)} {
		}

		template <std::invocable Fn>
		[[nodiscard]] DelayAction(ActionParam data, Fn&& func)
			: progress(data.delay.count(), data.noSuspend ? data.delay.count() : 0), repeatCount{data.repeat}, action{std::forward<Fn>(func)} {
		}

		[[nodiscard]] constexpr bool hasRepeat() const noexcept{
			return repeatCount > 0;
		}

		bool update(const Core::Tick deltaTick) {
			progress.update(deltaTick);

			while(progress){
				assert(action != nullptr);
				action();
				repeatedCount++;

				if(repeatCount > repeatedCount){
					progress.time = 0.f;
					// progress.time -= progress.lifetime;
				}else{
					return true;
				}
			}

			return false;
		}
	};

	class DelayActionManager {
		using PriorityIndex = std::underlying_type_t<ActionPriority>;

		struct ActionGroup{
			std::vector<DelayAction> actives{};
			std::vector<DelayAction> pending{};
			mutable std::mutex mtx{};

			template <std::invocable<> Func>
			void launch(const ActionParam param, Func&& action){
				std::lock_guard lockGuard{mtx};
				pending.emplace_back(param, std::forward<Func>(action));
			}

			void dump(){
				// std::lock_guard lockGuard{mtx};

				actives.reserve(actives.size() + pending.size());
				std::ranges::move(std::move(pending), std::back_inserter(actives));
				pending.clear();
			}

			void consume(Core::Tick deltaTick){
				ext::algo::erase_if_unstable(actives, [deltaTick](DelayAction& action){
					return action.update(deltaTick);
				});
			}
		};

		[[nodiscard]] ActionGroup& getGroupAt(const ActionPriority priority) noexcept{
			return taskGroup[std::to_underlying(priority)];
		}

		[[nodiscard]] const ActionGroup& getGroupAt(const ActionPriority priority) const noexcept{
			return taskGroup[std::to_underlying(priority)];
		}

		std::array<ActionGroup, std::to_underlying(ActionPriority::last) + 1> taskGroup{};

	public:
		ActionPriority lowestPriority = ActionPriority::last;

		[[nodiscard]] DelayActionManager() {
			for (auto & [actives, toAdd, _] : taskGroup){
				actives.reserve(300);
				toAdd.reserve(100);
			}
		}

		template <std::invocable<> Func>
		void launch(const ActionPriority priority, const ActionParam param, Func&& action) {
			if(priority > lowestPriority)return;

			getGroupAt(priority).launch(param, std::forward<Func>(action));
		}

		void clear() {
			for (auto & [actives, toAdd, _] : taskGroup){
				actives.clear();
				toAdd.clear();
			}
		}

		/**
		 * @brief Only @link ActionPriority::unignorable @endlink action will be applied
		 */
		void applyAndClear(){
			{
				ActionGroup& group = taskGroup[std::to_underlying(ActionPriority::unignorable)];
				group.dump();
				group.consume(Core::Tick{std::numeric_limits<float>::infinity()});
			}

			for(std::size_t i = std::to_underlying(ActionPriority::unignorable); i <= std::to_underlying(lowestPriority); ++i){
				ActionGroup& group = taskGroup[i];
				group.actives.clear();
				group.pending.clear();
			}
		}

		void update(const Core::Tick deltaTick) {
			for(std::size_t i = 0; i <= std::to_underlying(lowestPriority); ++i){
				ActionGroup& group = taskGroup[i];

				group.dump();
				group.consume(deltaTick);
			}
		}
	};
}

//
// Created by Matrix on 2024/10/17.
//

export module Game.World.EntityGroup;

export import Game.Entity;
export import ext.open_hash_map;

import std;

export namespace Game{
	template <std::derived_from<Entity> T>
	struct EntityGroup{
		ext::open_hash_map<EntityID, std::shared_ptr<T>> entities{EntityID{}, 1024};

		template <std::invocable<T&> Fn>
		void each(Fn fn){
			for (std::shared_ptr<T>& entity : entities | std::views::values){
				std::invoke(fn, *entity);
			}
		}

		template <std::invocable<T&> Fn>
		void each_par(Fn fn){
			auto view = entities | std::views::values;
			std::for_each(std::execution::par, std::ranges::begin(view), std::ranges::end(view), [fn](std::shared_ptr<T>& entity){
				std::invoke(fn, *entity);
			});
		}

		void update(UpdateTick tick){
			auto view = entities | std::views::values;
			std::for_each(std::execution::par, std::ranges::begin(view), std::ranges::end(view), [tick](std::shared_ptr<T>& entity){
				entity->update(tick);
			});
		}
	};
}
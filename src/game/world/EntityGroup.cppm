module;

#include <cassert>

export module Game.World.EntityGroup;

export import Game.Entity;
export import ext.open_hash_map;

import std;

namespace Game{
	template <typename T>
	struct vector_multi_thread{
	private:
		mutable std::mutex mutex{};
		std::vector<T> items{};

	public:
		auto& raw() noexcept{
			return items;
		}

		template <typename Ty>
		void push(Ty&& item){
			std::unique_lock lock(mutex);
			items.push_back(std::forward<Ty>(item));
		}

		[[nodiscard]] std::vector<T> dump() noexcept{
			std::unique_lock lock(mutex);
			auto rst = std::move(items);
			return rst;
		}

		void clear() noexcept{
			std::unique_lock lock(mutex);
			items.clear();
		}
	};

	// using T = std::string;

	export
	template <typename T>
	struct EntityGroup{
		using EntityType = T;
		using EntityPtr = std::shared_ptr<T>;

		using MapType = ext::open_hash_map<EntityID, EntityPtr>;
		MapType entities{EntityID{}, 1024};

	private:
		vector_multi_thread<EntityPtr> pendings{};
		vector_multi_thread<EntityPtr> expired{};
		vector_multi_thread<EntityID> toBeDeleted{};

	public:


		std::size_t size() const noexcept{
			return entities.size();
		}

		decltype(auto) getEntities() const noexcept{
			return entities | std::views::values | std::views::transform([](const EntityPtr& ptr) -> EntityType& {
				return *ptr;
			});
		}

		/**
		 * @brief thread safe
		 */
		void pend(EntityPtr&& ptr){
			pendings.push(std::move(ptr));
		}

		void dumpAll(){
			clearExpired();
			dumpMarkedDeletable();
			dumpPendings();
		}

		/**
		 * @brief thread safe
		 */
		void pend(const EntityPtr& ptr){
			pendings.push(ptr);
		}

		void notifyDelete(EntityID id){
			toBeDeleted.push(id);
		}

		/**
		 * @brief call in an exclusive thread
		 */
		void dumpPendings() requires (std::derived_from<T, Entity>){
			for (auto&& raw : pendings.raw()){
				Entity& entity = *raw;
				auto id = entity.getID();

				entity.setActivated();
				entities.try_emplace(id, std::move(raw));
			}
			pendings.clear();
		}

		void clearExpired() noexcept{
			if(expired.raw().size() > 128){
				expired.raw().clear();
			}
		}

		/**
		 * @thread_safety call in an exclusive thread
		 */
		void dumpMarkedDeletable() requires (std::derived_from<T, Entity>){
			for (EntityID id : toBeDeleted.raw()){
				typename MapType::iterator itr = entities.find(id);

				assert(itr != entities.end());

				Entity& entity = *itr->second;
				assert(entity.getState() == EntityState::deletable);

				pendings.raw().push_back(std::move(itr->second));
				entities.erase(itr);
			}

			toBeDeleted.clear();
		}

		/**
		 * @brief actively
		 * @thread_safety call in an exclusive thread
		 */
		void dumpDeletableBySearch() requires (std::derived_from<T, Entity>){
			for(typename MapType::iterator cur = entities.begin(); cur != entities.end(); ++cur){
				Entity& entity = *cur->second;
				// assert(entity.getState() != EntityState::pending && entity.getState() != EntityState::expired);

				if(entity.getState() == EntityState::deletable){
					// entity.setExpired();
					pendings.raw().push_back(std::move(cur->second));
					entities.erase(cur);
				}
			}


		}

		template <std::invocable<T&> Fn>
		void each(Fn fn){
			for (EntityPtr& entity : entities | std::views::values){
				std::invoke(fn, *entity);
			}
		}

		template <std::invocable<T&> Fn>
		void each_par(Fn fn){
			auto view = entities | std::views::values;
			std::for_each(std::execution::par, std::ranges::begin(view), std::ranges::end(view), [fn](EntityPtr& entity){
				std::invoke(fn, *entity);
			});
		}

		void update_par(UpdateTick tick) requires (std::derived_from<T, Entity>){
			auto view = entities | std::views::values;
			std::for_each(std::execution::par, std::ranges::begin(view), std::ranges::end(view), [tick](EntityPtr& entity){
				entity->update(tick);
			});
		}
	};
}
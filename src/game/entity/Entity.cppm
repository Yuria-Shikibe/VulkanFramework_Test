//
// Created by Matrix on 2024/10/16.
//

export module Game.Entity;

import std;
export import Game.Entity.Unit;

export namespace Game{
	struct WorldState;

	// struct ReferenceCount{
	// 	using size_type = unsigned;
	//
	// 	size_type mainCount;
	// 	size_type weakCount;
	// };
	//

	enum struct EntityState{
		// pending,
		activated,
		deactivated,
		deletable,
		// expired,
	};

	struct Entity : public std::enable_shared_from_this<Entity>{
	protected:
		EntityState state{};

	public:
		[[nodiscard]] Entity() noexcept = default;

		// void setExpired() noexcept{
		// 	state = EntityState::expired;
		// }

		void setActivated() noexcept{
			state = EntityState::activated;
		}

		[[nodiscard]] EntityID getID() const noexcept{
			return std::bit_cast<EntityID>(this);
		}

		virtual ~Entity() = default;

		virtual void update(UpdateTick deltaTick){

		}

		[[nodiscard]] EntityState getState() const noexcept{
			return state;
		}

		[[nodiscard]] bool isStateValid() const noexcept{
			return state == EntityState::activated;
		}

		/**
		 * TODO using erase instead of delete?
		 * @brief notify given world state that this entity can be ACTIVELY deleted
		 *
		 * cannot be cancelled after notification
		 */
		virtual void notifyDelete(WorldState& worldState) noexcept;

		//..i want deucing this
		template <std::derived_from<Entity> T>
		std::shared_ptr<T> as(){
#if DEBUG_CHECK
			return std::dynamic_pointer_cast<T>(shared_from_this());
#else
			return std::static_pointer_cast<T>(shared_from_this());
#endif
		}

	};
}

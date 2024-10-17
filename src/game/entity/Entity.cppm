//
// Created by Matrix on 2024/10/16.
//

export module Game.Entity;

import std;
export import Game.Entity.Unit;

export namespace Game{
	// struct ReferenceCount{
	// 	using size_type = unsigned;
	//
	// 	size_type mainCount;
	// 	size_type weakCount;
	// };
	//
	// struct

	class RemoveCallalble {
	public:
		virtual ~RemoveCallalble() = default;

		// virtual void postRemovePrimitive(Game::Entity* entity) = 0;

		// virtual void postAddPrimitive(Game::Entity* entity) = 0;
	};

	enum struct EntityState{
		pending,
		activated,
		deactivated,
		deletable,
		expired,
	};

	struct Entity : std::enable_shared_from_this<Entity>{
	protected:
		EntityState state{};

	public:
		[[nodiscard]] EntityID getID() const noexcept{
			return std::bit_cast<EntityID>(this);
		}

		virtual ~Entity() = default;

		virtual void update(UpdateTick deltaTick) = 0;

		[[nodiscard]] bool isExpired() const noexcept{
			return state == EntityState::expired;
		}
		void setExpired() noexcept{
			state = EntityState::expired;
		}

		[[nodiscard]] EntityState getState() const noexcept{
			return state;
		}

		[[nodiscard]] bool isStateValid() const noexcept{
			return state == EntityState::activated;
		}

	};
}

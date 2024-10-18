module;

export module Game.World.RealEntity;


import Geom.QuadTree.Interface;

export import Game.Entity;
export import Game.Entity.HitBox;
export import Game.Entity.Properties.Motion;
export import Game.World.Drawable;

export import Game.Faction;

import Game.Graphic.Draw.Universal;

import std;

namespace Game{

	export
	class RealEntity :
		public Entity,
		public Geom::QuadTreeAdaptable<RealEntity, float>,
		public Drawable
	{
	public:

		struct Intersections{
			struct Collision{ // NOLINT(*-pro-type-member-init)
				const RealEntity* other;
				const CollisionData* data;
			};

			std::vector<Collision> collisions{};

			[[nodiscard]] bool collided() const noexcept{
				return !collisions.empty();
			}

			void push(const RealEntity* other, const CollisionData* data){
				collisions.emplace_back(other, data);
			}

			[[jetbrains::has_side_effects]]
			bool filter() noexcept{
				std::erase_if(collisions, [](const Collision& c){
					return c.data->expired;
				});

				return collided();
			}
		};

		MechMotionProp motion{};
		RigidProp rigidProp{};

		Hitbox hitbox{};
		Intersections intersections{};

		const Faction* faction{};

		float zLayer{};
		float zThickness{};

		[[nodiscard]] bool preProcessCollisions() noexcept{
			return intersections.filter();
		}

		virtual void postProcessCollisions() noexcept{
			//TODO do physics here
			intersections.collisions.clear();
		}

		// ReSharper disable once CppHidingFunction
		[[nodiscard]] bool containsPoint(const Geom::Vec2 point) const noexcept{
			return hitbox.contains(point);
		}

		// ReSharper disable once CppHidingFunction
		[[nodiscard]] Geom::OrthoRectFloat getBound() const noexcept{
			return hitbox.getMaxWrapBound();
		}

		void update(UpdateTick deltaTick) override{
			hitbox.updateHitboxWithCCD(motion.trans);
		}

		void notifyDelete(WorldState& worldState) noexcept override;

		/**
		 * @brief asymmetric, unique part, general test should be done in between version
		 * @return false if no collision should happen
		 */
		[[nodiscard]] virtual bool ignoreCollisionTo(const RealEntity& other) const noexcept{
			return false;
		}

		[[nodiscard]] bool ignoreCollisionBetween(const RealEntity& other) const noexcept{
			const bool test = false;
			return test || this->ignoreCollisionTo(other) || other.ignoreCollisionTo(*this);
		}

		// ReSharper disable once CppHidingFunction
		[[nodiscard]] bool roughIntersectWith(const RealEntity& object) const noexcept{
			if(!isStateValid() || !object.isStateValid())return false;
			if(std::abs(zLayer - object.zLayer) > zThickness + object.zThickness)return false;

			if(ignoreCollisionBetween(*this))return false;

			return hitbox.collideWithRough(object.hitbox);
		}

		// ReSharper disable once CppHidingFunction
		[[nodiscard]] bool exactIntersectWith(const RealEntity& object) const noexcept{
			const auto data = hitbox.collideWithExact(object.hitbox);

			if(data && !data->indices.empty()){
				return true;
			}

			return false;

		}


		// ReSharper disable once CppHidingFunction
		void testIntersectionWith(const RealEntity& object){
			const auto data = hitbox.collideWithExact(object.hitbox);

			if(data && !data->indices.empty()){
				intersections.push(&object, data);
			}
		}

	public:
		void draw() const override{
			Graphic::Draw::hitbox(hitbox, zLayer);
		}

		[[nodiscard]] Geom::OrthoRectFloat getClipRegion() const noexcept override{
			return hitbox.getMinWrapBound();
		}


	};


}

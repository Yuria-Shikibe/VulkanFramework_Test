//
// Created by Matrix on 2024/10/16.
//

export module Game.World.RealEntity;

export import Game.Faction;

import Geom.QuadTree.Interface;

export import Game.Entity;
export import Game.Entity.HitBox;
export import Game.Entity.Properties.Motion;

import std;

export namespace Game{

	class RealEntity :
		public Entity,
		public Geom::QuadTreeAdaptable<RealEntity, float>
	{
		struct Intersections{
			struct Collision{
				const RealEntity* other;
				const CollisionData* data;
			};

			mutable std::vector<Collision> collisions{};

			[[nodiscard]] bool collided() const noexcept{
				return !collisions.empty();
			}

			void push(const RealEntity* other, const CollisionData* data) const{
				collisions.emplace_back(other, data);
			}

			[[jetbrains::has_side_effects]]
			bool filter() const noexcept{
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


		bool preProcessCollisions() const noexcept{
			return intersections.filter();
		}

		void postProcessCollisions() noexcept{
			// return intersections.filter();
		}

		// ReSharper disable once CppHidingFunction
		[[nodiscard]] bool containsPoint(const Geom::Vec2 point) const noexcept{
			return hitbox.contains(point);
		}

		// ReSharper disable once CppHidingFunction
		[[nodiscard]] Geom::OrthoRectFloat getBound() const noexcept{
			return hitbox.getMaxWrapBound();
		}

		// ReSharper disable once CppHidingFunction
		[[nodiscard]] bool roughIntersectWith(const RealEntity& object) const noexcept{
			if(!isStateValid() || !object.isStateValid())return false;
			if(std::abs(zLayer - object.zLayer) > zThickness + object.zThickness)return false;

			// if(ignoreCollisionTo(object) || object.ignoreCollisionTo(*this))return false;

			return hitbox.collideWithRough(object.hitbox);
		}

		// ReSharper disable once CppHidingFunction
		[[nodiscard]] bool exactIntersectWith(const RealEntity& object) const{
			// const bool needInterscetPointCalculation_subject = requiresCollisionIntersection();
			// const bool needInterscetPointCalculation_object = requiresCollisionIntersection();

			const auto data = hitbox.collideWithExact(object.hitbox);

			if(data && !data->indices.empty()){
				intersections.push(&object, data);
				return true;
			}

			return false;

		}
	};
}
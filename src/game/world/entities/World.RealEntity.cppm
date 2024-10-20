module;

#include <cassert>

export module Game.World.RealEntity;


import Geom.QuadTree.Interface;

export import Game.Entity;
export import Game.Entity.HitBox;
export import Game.Entity.Properties.Motion;
export import Game.World.Drawable;

export import Game.Faction;

import Game.Graphic.Draw.Universal;
import Math;
import Geom;

import std;

namespace Game{

	export
	class RealEntity :
		public Entity,
		public Geom::QuadTreeAdaptable<class RealEntity, float>,
		public Drawable
	{
	public:

		struct CollisionTestContext{
			struct Collision{ // NOLINT(*-pro-type-member-init)
				const RealEntity* other;
				const CollisionData* data;

				[[nodiscard]] bool isExpired() const noexcept{
					assert(data != nullptr);
					return data->expired;
				}
			};

			struct Intersection{
				Geom::Vec2 pos{};
				Geom::NorVec2 normal{};
				CollisionIndex index{};
				//TODO hitbox index??
			};

			struct CollisionPostData{ // NOLINT(*-pro-type-member-init)
				const RealEntity* other{};
				Geom::Vec2 correctionVec{};
				Intersection mainIntersection{};
				std::vector<Intersection> intersections{};
			};

			std::vector<const RealEntity*> lastCollided{};

			std::vector<Collision> collisions{};

			std::vector<CollisionPostData> postData{};

			[[nodiscard]] bool collided() const noexcept{
				return !collisions.empty();
			}

			void push(const RealEntity* other, const CollisionData* data){
				collisions.emplace_back(other, data);
			}

			[[jetbrains::has_side_effects]]
			bool filter() noexcept{
				std::erase_if(collisions, [](const Collision& c){
					//OPTM why it becomes empty???
					return c.isExpired() || c.data->empty();
				});

				return collided();
			}

			void processIntersections(RealEntity& subject) noexcept;
		};

		MechMotionProp motion{};
		RigidProp rigidProp{};
	protected:
		Geom::Vec2 collisionTestTempVel{};
		Geom::Vec2 collisionTestTempPos{};

	public:
		Hitbox hitbox{};
		CollisionTestContext collisionContext{};

		const Faction* faction{};

		float zLayer{};
		float zThickness{};

		// ---------------------------------------------------------------------
		// Collision Test Methods BEGIN
		// ---------------------------------------------------------------------

		[[nodiscard]] bool postProcessCollisions_0() noexcept{
			//TODO is this necessary?
			auto backTrace = hitbox.getBackTraceMove();
			// std::println("unit {}", hitbox.getBackTraceUnitMove());
			// std::println("trace {}", backTrace);

			motion.trans.vec += backTrace;
			hitbox.trans.vec = motion.trans.vec;
			motion.check();
			return collisionContext.filter();
		}

		void clearCollisionData() const noexcept{
			hitbox.clearCollided();
		}

		void postProcessCollisions_1() noexcept{
			collisionContext.processIntersections(*this);
		}

		void postProcessCollisions_2() noexcept;

		void postProcessCollisions_3(UpdateTick delta) noexcept;

		// ---------------------------------------------------------------------
		// Collision Test Methods END
		// ---------------------------------------------------------------------

		// ReSharper disable once CppHidingFunction
		[[nodiscard]] bool containsPoint(const Geom::Vec2 point) const noexcept{
			return hitbox.contains(point);
		}

		// ReSharper disable once CppHidingFunction
		[[nodiscard]] Geom::OrthoRectFloat getBound() const noexcept{
			return hitbox.getMaxWrapBound();
		}

		void update(UpdateTick deltaTick) override{
			motion.applyAndReset(deltaTick);

			motion.vel.vec.lerp({}, 0.05f * deltaTick);
			motion.vel.rot /= 1.125f;

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
			const auto* data = hitbox.collideWithExact(object.hitbox, true, true);

			if(data && !data->indices.empty()){
				auto t = data->indices.top();
				collisionContext.push(&object, data);
			}
		}

	public:
		void draw() const override;

		[[nodiscard]] Geom::OrthoRectFloat getClipRegion() const noexcept override{
			return hitbox.getMinWrapBound();
		}

	private:
		[[nodiscard]] constexpr Geom::Vec2 collideVelAt(const Geom::Vec2 dst) const {
			return collisionTestTempVel + dst.cross(motion.vel.rot * Math::DEGREES_TO_RADIANS);
		}

	public:

		virtual void calCollideTo(
			const RealEntity* object,
			CollisionTestContext::Intersection intersection,
			const float energyScale,
			UpdateTick deltaTick
		) {
			//Pull in to correct calculation

			// if(object->isOverrideCollisionTo(this)) {
			// 	const_cast<Game::RealityEntity*>(object)->overrideCollisionTo(this, data.intersection);
			// 	return;
			// }

			using Geom::Vec2;

			//Origin Point To Hit Point
			const Vec2 dstToSubject = (intersection.pos - motion.trans.vec).limitMin(10);
			const Vec2 dstToObject  = (intersection.pos - object->motion.trans.vec).limitMin(10);

			const Vec2 subjectVel = collideVelAt(dstToSubject);
			const Vec2 objectVel = object->collideVelAt(dstToObject);

			const Vec2 relVel = (objectVel - subjectVel) * energyScale;

			//TODO overlap quit process
			if(relVel.isZero())return;

			const Vec2 collisionNormalVec =
				intersection.normal;
				// object->hitbox.getNormalAt(intersection.pos, intersection.index.object).normalize();

			Vec2 collisionTangentVec = collisionNormalVec.copy().rotateRT_counterClockwise();
			if(collisionTangentVec.dot(relVel) < 0)collisionTangentVec.reverse();

			const float scaledMass = 1 / rigidProp.inertialMass + 1 / object->rigidProp.inertialMass;

			//TODO cache this?
			const float subjectRotationalInertia = this->getRotationalInertia();
			const float objectRotationalInertia = object->getRotationalInertia();

			const Vec2 vertHitVel{relVel.copy().project_nonNormalized(collisionNormalVec)};
			const Vec2 horiHitVel{(relVel - vertHitVel)};

			Vec2 impulseNormal{vertHitVel};
			impulseNormal *= (1 + (rigidProp.restitution - object->rigidProp.restitution)) /
			(scaledMass +
				Math::sqr(dstToObject.cross(collisionNormalVec)) / objectRotationalInertia +
				Math::sqr(dstToSubject.cross(collisionNormalVec)) / subjectRotationalInertia
			);

			const Vec2 impulseTangent = horiHitVel.sign().cross(std::min(
				(rigidProp.frictionCoefficient + object->rigidProp.frictionCoefficient) * impulseNormal.length(), //u * Fn
				horiHitVel.length() /
			(scaledMass +
				Math::sqr(dstToObject.cross(collisionTangentVec)) / objectRotationalInertia +
				Math::sqr(dstToSubject.cross(collisionTangentVec)) / subjectRotationalInertia)
			));

			Vec2 additional = collisionNormalVec * impulseNormal.length() + collisionTangentVec * impulseTangent.length();

			// additional.limit2(relVel.length2());

			if(std::ranges::contains(collisionContext.lastCollided, object)){
				std::println(std::cerr, "duplicated collided");


			}else{
				motion.accel.vec += additional / rigidProp.inertialMass;
				motion.accel.rot += dstToSubject.cross(additional) / subjectRotationalInertia * Math::RADIANS_TO_DEGREES;
				motion.vel += motion.accel * deltaTick;
			}

			// ----- correction -----

			// collision correction
			if(hitbox.components[intersection.index.subject].box.contains(intersection.pos)){
				//hit correction, yes this makes no sense
				// additional.reverse();//.scl(.35f);
				auto approach = object->motion.pos() - motion.pos();
				bool b = collisionTestTempVel.length() * rigidProp.inertialMass < object->collisionTestTempVel.length() * object->rigidProp.inertialMass;
				if(b) {
					auto correction = additional * .35f;//.copy().setLength2(hitbox.estimateMaxLength2());
					if(correction.dot(approach) > 0){
						correction.reverse();
					}
					collisionTestTempPos.add(correction.scl(0.005f));
					motion.vel.vec.add(correction.scl(0.5f));
				}
			}

			// if(motion.vel.vec.dot(relVel) > 0){
			// 	motion.vel.vec = additional * (/*deltaTick * */VelScale / rigidProp.inertialMass);
			// }
		}

		[[nodiscard]] constexpr float getRotationalInertia() const {
			return hitbox.getRotationalInertia(rigidProp.inertialMass, rigidProp.rotationalInertiaScale);
		}
	};


}

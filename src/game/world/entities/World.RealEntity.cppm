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

		struct Manifold{


			struct Collision{ // NOLINT(*-pro-type-member-init)
				const RealEntity* other;
				CollisionData data;
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
			};

			bool underCorrection{};

			std::vector<const RealEntity*> lastCollided{};

			std::vector<Collision> collisions{};

			std::vector<CollisionPostData> postData{};

			[[nodiscard]] bool collided() const noexcept{
				return !collisions.empty();
			}

			// void push(const RealEntity* other, const CollisionData* data){
			// 	collisions.emplace_back(other, data);
			// }

			[[jetbrains::has_side_effects]]
			bool filter(const unsigned backtraceIndex) noexcept{
				std::erase_if(collisions, [backtraceIndex](const Collision& c){
					return c.data.transition > backtraceIndex;
				});

				return collided();
			}

			void markLast(){
				for (const auto & data : postData){
					if(auto itr = std::ranges::find(lastCollided, data.other); itr != lastCollided.end()){
						//notify itr entering collision

						lastCollided.erase(itr);
					}
				}

				for (const auto & data : lastCollided){
					//notify data exiting collision
				}

				lastCollided = {std::from_range, postData | std::views::transform(&Manifold::CollisionPostData::other)};

			}

			void processIntersections(RealEntity& subject) noexcept;

			void clear(){
				std::destroy_at(this);
				std::construct_at(this);
			}
		};

		MechMotionProp motion{};
		RigidProp rigidProp{};
	protected:
		Geom::Transform collisionTestTempVel{};
		Geom::Vec2 collisionTestTempPos{};

	public:
		Geom::Vec2 last{};

	public:
		Hitbox hitbox{};
		Manifold manifold{};

		const Faction* faction{};

		float zLayer{};
		float zThickness{};

		// ---------------------------------------------------------------------
		// Collision Test Methods BEGIN
		// ---------------------------------------------------------------------

		auto postProcessCollisions_0() noexcept{
			//TODO is this necessary?
			bool rst = manifold.filter(hitbox.getBackTraceIndex());

			if(rst){
				if(!manifold.underCorrection){
					const auto backTrace = hitbox.getBackTraceMove();

					motion.trans.vec += backTrace;
					hitbox.trans.vec = motion.trans.vec;
				}
			}else{
				manifold.underCorrection = false;
			}



			return rst;
		}

		void postProcessCollisions_1() noexcept{
			manifold.processIntersections(*this);
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
			motion.vel.rot /= 1.0125f;

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
			// if(std::ranges::contains(collisionContext.lastCollided, &object))return false;

			return hitbox.collideWithRough(object.hitbox);
		}

		// ReSharper disable once CppHidingFunction
		[[nodiscard]] bool exactIntersectWith(const RealEntity& object) const noexcept{
			return hitbox.collideWithExact(object.hitbox, {}, false);
		}


		// ReSharper disable once CppHidingFunction
		bool testIntersectionWith(const RealEntity& object){
			return hitbox.collideWithExact(object.hitbox, [this, &object](const unsigned index) {
				return &manifold.collisions.emplace_back(&object, CollisionData{index}).data;
			}, false);
			//
			// if(data && !data->indices.empty()){
			// 	auto t = data->indices.top();
			// 	collisionContext.push(&object, data);
			// }
		}

	public:
		void draw() const override;

		[[nodiscard]] Geom::OrthoRectFloat getClipRegion() const noexcept override{
			return hitbox.getMinWrapBound();
		}

	private:
		[[nodiscard]] constexpr Geom::Vec2 collideVelAt(const Geom::Vec2 dst) const {
			return motion.vel.vec - dst.cross(Math::clampRange(static_cast<float>(motion.vel.rot) * Math::DEGREES_TO_RADIANS, 15.f));
		}

	public:

		virtual void calCollideTo(
			const RealEntity& object,
			Manifold::Intersection intersection,
			const float energyScale,
			UpdateTick deltaTick
		) {
			assert(&object != this);
			//Pull in to correct calculation

			// if(object->isOverrideCollisionTo(this)) {
			// 	const_cast<Game::RealityEntity*>(object)->overrideCollisionTo(this, data.intersection);
			// 	return;
			// }
			// if(std::ranges::contains(collisionContext.lastCollided, &object))return;

			using Geom::Vec2;

			//Origin Point To Hit Point
			const Vec2 dstToSubject = (intersection.pos - motion.trans.vec);
			const Vec2 dstToObject  = (intersection.pos - object.motion.trans.vec);

			const Vec2 subjectVel = collideVelAt(dstToSubject);
			const Vec2 objectVel = object.collideVelAt(dstToObject);

			const Vec2 relVel = (objectVel - subjectVel) * energyScale;

			const Vec2 approach = intersection.normal;//motion.pos() - object.motion.pos();

			Vec2 correctionVec{};

			bool contains = std::ranges::contains(manifold.lastCollided, &object);

			// if(contains && approach.dot(relVel) > 0.f){
			// 	return;
			// }

			//TODO correction
			//TODO overlap quit process
			if(relVel.length2() > Math::sqr(2)){
				const Vec2 collisionNormalVec = intersection.normal;

				const float scaledMass = 1 / rigidProp.inertialMass + 1 / object.rigidProp.inertialMass;
				const float subjectRotationalInertia = this->getRotationalInertia();
				const float objectRotationalInertia = object.getRotationalInertia();

				const Vec2 vertHitVel{relVel.copy().project_nonNormalized(collisionNormalVec)};
				const Vec2 horiHitVel{(relVel - vertHitVel)};

				const Vec2 collisionTangentVec = horiHitVel.copy().normalize();

				const Vec2 impulseNormal = vertHitVel * (1 + std::min(rigidProp.restitution, object.rigidProp.restitution)) /
				(scaledMass +
					Math::sqr(dstToObject.cross(collisionNormalVec)) / objectRotationalInertia +
					Math::sqr(dstToSubject.cross(collisionNormalVec)) / subjectRotationalInertia
				);

				Vec2 impulseTangent = horiHitVel *
					horiHitVel.length() /
					(scaledMass +
						Math::sqr(dstToObject.cross(collisionTangentVec)) / objectRotationalInertia +
						Math::sqr(dstToSubject.cross(collisionTangentVec)) / subjectRotationalInertia);

				impulseTangent.limitMax(std::sqrt(rigidProp.frictionCoefficient * object.rigidProp.frictionCoefficient) * impulseNormal.length());

				Vec2 impulse = impulseNormal + impulseTangent;

				last = impulse;

				collisionTestTempVel.vec += impulse / rigidProp.inertialMass;
				collisionTestTempVel.rot += dstToSubject.cross(impulse) / subjectRotationalInertia * Math::RADIANS_TO_DEGREES * 0.75f;

				correctionVec = impulse;
			}

			// if(contains && approach.dot(motion.vel.vec) > 0.f){
			// 	correctionVec *= .35f;
			// }

			correction:

			// collision correction
			if(contains && rigidProp.inertialMass <= object.rigidProp.inertialMass){
				// if(rigidProp.inertialMass == object.rigidProp.inertialMass){
				// 	if(std::less{}(this, &object))return;
				// }

				auto unitLength = correctionVec;

				if(unitLength.equalsTo(Geom::ZERO) || unitLength.dot(approach) < .0f){
					unitLength = approach.copy().setLength(4);
				}else{
					unitLength.limitClamp(3, 12);
				}

				auto sbjBox = static_cast<Geom::RectBoxBrief>(hitbox.components[intersection.index.subject].box);
				const auto& objBox = object.hitbox.components[intersection.index.object].box;

				sbjBox.move(motion.vel.vec * deltaTick);
				Geom::Vec2 begin{sbjBox.v0};

				while(sbjBox.overlapRough(objBox) && sbjBox.overlapExact(objBox)){
					sbjBox.move(unitLength);
				}

				unitLength.setLength(4.);
				collisionTestTempVel.vec += unitLength / 2;
				collisionTestTempPos += sbjBox.v0 - begin + unitLength;

				manifold.underCorrection = true;
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

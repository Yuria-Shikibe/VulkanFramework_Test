module;

#include <cassert>

module Game.World.RealEntity;

import Game.World.State;
import Geom;

import Assets.Fx;
import Graphic.Effect.Manager;
import Graphic.Color;


constexpr float MinimumSuccessAccuracy = 1. / 32.;
constexpr float MinimumFailedAccuracy = 1. / 32.;

Geom::Vec2 approachTest(
	Geom::QuadBox& sbjBox, const Geom::Vec2 sbjMove,
	Geom::QuadBox& objBox, const Geom::Vec2 objMove,
	const Geom::Vec2 sbjNv1, const Geom::Vec2 sbjNv2,
	const Geom::Vec2 objNv1, const Geom::Vec2 objNv2
){
	// assert(sbjBox.overlapExact(objBox, sbjNv1, sbjNv2, objNv1, objNv2));

	const Geom::Vec2 beginning = sbjBox.v0;
	float currentTestStep = .5f;
	float failedSum = .0f;

	bool lastTestResult = sbjBox.overlapRough(objBox) && sbjBox.overlapExact(objBox, sbjNv1, sbjNv2, objNv1, objNv2);

	while(true){
		sbjBox.move(sbjMove * currentTestStep);
		objBox.move(objMove * currentTestStep);

		if(sbjBox.overlapRough(objBox) && sbjBox.overlapExact(objBox, sbjNv1, sbjNv2, objNv1, objNv2)){
			if(std::abs(currentTestStep) < MinimumSuccessAccuracy){
				break;
			}

			//continue backtrace
			currentTestStep /= lastTestResult ? 2.f : -2.f;
			failedSum = .0f;

			lastTestResult = true;
		}else{
			if(std::abs(currentTestStep) < MinimumFailedAccuracy){
				if(lastTestResult){
					failedSum = -currentTestStep;
				}
				//backtrace to last succeed position
				sbjBox.move(sbjMove * failedSum);
				objBox.move(objMove * failedSum);

				break;
			}

			//failed, perform forward trace
			failedSum -= currentTestStep;
			currentTestStep /= lastTestResult ? -2.f : 2.f;

			lastTestResult = false;
		}
	}

	// assert(sbjBox.overlapExact(objBox, sbjNv1, sbjNv2, objNv1, objNv2));

	return sbjBox.v0 - beginning;
}

void Game::RealEntity::Manifold::processIntersections(RealEntity& subject) noexcept{
	postData.clear();

	// if(underCorrection)return;

	for (const Collision& data : collisions){

		assert(data.data.transition <= subject.hitbox.getBackTraceIndex());
		assert(!data.data.empty());

		for (const auto index : data.data.indices){
			if(auto itr = std::ranges::find(postData, data.other, &CollisionPostData::other);
							itr != postData.end())continue;

			const auto& sbj = subject.hitbox.components[index.subject].box;
			const auto& obj = data.other->hitbox.components[index.object].box;

			CollisionPostData* validPostData{};

			auto sbjBox = static_cast<Geom::QuadBox>(sbj);
			auto objBox = static_cast<Geom::QuadBox>(obj);

			sbjBox.move(subject.hitbox.getBackTraceMove());
			objBox.move(data.other->hitbox.getBackTraceMove());

			const auto sbjMove = subject.hitbox.getBackTraceUnitMove();
			const auto objMove = data.other->hitbox.getBackTraceUnitMove();

			if(underCorrection || sbjMove.length2() + objMove.length2() < Math::sqr(15) || std::ranges::contains(lastCollided, data.other)){
				validPostData = &postData.emplace_back(data.other);
			} else{
				const auto correction =
					approachTest(
						sbjBox, subject.hitbox.getBackTraceUnitMove(),
						objBox, data.other->hitbox.getBackTraceUnitMove(),
						sbj.getNormalU(), sbj.getNormalV(), obj.getNormalU(), obj.getNormalV());

				validPostData = &postData.emplace_back(data.other, correction);
			}

			const auto avg = Geom::rectRoughAvgIntersection(sbjBox, objBox);

			// Assets::Fx::CircleOut.launch({
			// 							.zLayer = 0.2f,
			// 							.trans = {avg},
			// 							.color = ::Graphic::Colors::AQUA_SKY,
			// 						});

			validPostData->mainIntersection = {
				.pos = avg,
				.normal = Geom::avgEdgeNormal(avg, objBox).normalize(),
				.index = index
			};
		}
	}


	collisions.clear();
}


void Game::RealEntity::notifyDelete(WorldState& worldState) noexcept{
	//TODO notify exiting collision?
	Entity::notifyDelete(worldState);

	worldState.realEntities.notifyDelete(getID());
}

void Game::RealEntity::draw() const{
	Graphic::Draw::realEntity(*this);
}

void Game::RealEntity::postProcessCollisions_2() noexcept{
	//TODO ovserve whether this step is ignorable

	if(!manifold.postData.empty()){
		Geom::Vec2 off{};
		for(const auto& post_data : manifold.postData){
			off += post_data.correctionVec;
		}

		off /= manifold.postData.size();

		if(off.length2() > 1){
			// std::println(std::cerr, "{}", off);

			hitbox.trans.vec = (motion.trans.vec += off);
		}
	}

	collisionTestTempPos = {};
	collisionTestTempVel = {};

	//TODO necessary to update it at here?
	// hitbox.updateHitbox(motion.trans);
}

void Game::RealEntity::postProcessCollisions_3(const UpdateTick delta) noexcept{
	for (const auto& data : manifold.postData){
		calCollideTo(*data.other, data.mainIntersection, 1 / manifold.postData.size(), delta);
	}

	motion.trans.vec += collisionTestTempPos;
	motion.vel += collisionTestTempVel;
	motion.vel.rot.clamp(20.f);

	manifold.markLast();
	//TODO do physics here
	// collisionContext.processIntersections(*this);

}

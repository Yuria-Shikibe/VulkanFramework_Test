module;

#include <cassert>

module Game.World.RealEntity;

import Game.World.State;
import Geom;


constexpr float MinimumSuccessAccuracy = 0.25f / 4;
constexpr float MinimumFailedAccuracy = 0.0625 / 2;

Geom::Vec2 approachTest(
	Geom::QuadBox& sbjBox, const Geom::Vec2 sbjMove,
	Geom::QuadBox& objBox, const Geom::Vec2 objMove,
	const Geom::Vec2 sbjNv1, const Geom::Vec2 sbjNv2,
	const Geom::Vec2 objNv1, const Geom::Vec2 objNv2
){
	const Geom::Vec2 beginning = sbjBox.v0;
	float currentTestStep = .5f;
	float failedSum = .0f;

	bool lastTestResult = true;

	while(true){
		sbjBox.move(sbjMove * currentTestStep);
		objBox.move(objMove * currentTestStep);

		if(sbjBox.overlapRough(objBox) && sbjBox.overlapExact(objBox, sbjNv1, sbjNv2, objNv1, objNv2)){
			failedSum = .0f;
			if(std::abs(currentTestStep) < MinimumSuccessAccuracy){
				break;
			}

			//continue backtrace
			currentTestStep /= lastTestResult ? 2.f : -2.f;

			lastTestResult = true;
		}else{
			if(std::abs(currentTestStep) < MinimumFailedAccuracy){
				//backtrace to last succeed position
				sbjBox.move(sbjMove * failedSum);
				objBox.move(objMove * failedSum);
				break;
			}

			//failed, perform forward trace
			currentTestStep /= lastTestResult ? -2.f : 2.f;

			failedSum -= currentTestStep;

			lastTestResult = false;
		}
	}

	return sbjBox.v0 - beginning;
}

void Game::RealEntity::CollisionTestContext::processIntersections(RealEntity& subject) noexcept{
	postData.clear();

	for (const Collision& data : collisions){
		assert(!data.isExpired());
		assert(!data.data->empty());

		for (const auto index : data.data->indices){

			const auto& sbj = subject.hitbox.components[index.subject].box;
			const auto& obj = data.other->hitbox.components[index.object].box;

			auto sbjBox = static_cast<Geom::QuadBox>(sbj);
			sbjBox.move(subject.hitbox.getBackTraceMove());

			auto objBox = static_cast<Geom::QuadBox>(obj);
			objBox.move(data.other->hitbox.getBackTraceMove());

			CollisionPostData* validPostData{};
			if(auto itr = std::ranges::find(postData, data.other, &CollisionPostData::other);
				itr != postData.end()){
				validPostData = std::to_address(itr);
				}else{
					const auto correction = approachTest(
						sbjBox, subject.hitbox.getBackTraceUnitMove(),
						objBox, data.other->hitbox.getBackTraceUnitMove(),
						sbj.getNormalU(), sbj.getNormalV(), obj.getNormalU(), obj.getNormalV());

					// std::println(std::cerr, "{:<20}, {}", correction.dot(subject.motion.vel.vec), correction.dot(subject.motion.vel.vec) < 0);

					validPostData = &postData.emplace_back(data.other, correction);
				}

			const auto rst = Geom::rectExactAvgIntersection(sbjBox, objBox);

			if(!rst){
				postData.pop_back();
				continue;
			}

			for (const auto & point : rst.points){
				validPostData->intersections.emplace_back(
					point.pos, Geom::avgEdgeNormal(point.pos, objBox).normalize(), index);
			}

			Geom::Vec2 avg = rst.avg();

			/*if(rst.points.size() >= 2){
				auto p1 = rst.points.bottom();
				auto p2 = rst.points.top();

				if(Geom::IntersectionResult::Info::onNearEdge(p1.edgeObj, p2.edgeObj)){
					const auto dst = p1.pos - p2.pos; //p1 -> p2

					const auto n11 = Geom::avgEdgeNormal(p1.pos, obj).normalize();
					const auto n12 = Geom::avgEdgeNormal(p2.pos, obj).normalize();

					const auto n21 = Geom::Vec::normalTo( dst, n11);
					const auto n22 = Geom::Vec::normalTo(-dst, n12);

					auto intersect = Geom::intersectionLine(p1.pos, p1.pos + n21, p2.pos, p2.pos + n22);

					static constexpr float FundamentalConst = 8; // (2 * sqrt(2)) ^ 2
					const float dst2 = intersect.dst2(avg) * FundamentalConst;

					if(dst2 > intersect.dst2(p1.pos) && dst2 > intersect.dst2(p2.pos)){
						avg = intersect;
					}
				}
			}*/

			validPostData->mainIntersection = {
				.pos = avg,
				.normal = -Geom::avgEdgeNormal(avg, objBox).normalize(),
				.index = index
			};

		}



	}

	for (const auto & data : postData){
		if(auto itr = std::ranges::find(lastCollided, data.other); itr != lastCollided.end()){
			//notify itr entering collision

			lastCollided.erase(itr);
		}
	}

	for (const auto & data : lastCollided){
		//notify data exiting collision
	}

	lastCollided = {std::from_range, postData | std::views::transform(&CollisionPostData::other)};

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
	//TODO obvserve whether this step is ignorable

	if(!collisionContext.postData.empty()){
		Geom::Vec2 off{};
		for(const auto& post_data : collisionContext.postData){
			off += post_data.correctionVec;
		}

		motion.trans.vec += off / collisionContext.postData.size();
	}

	collisionTestTempPos = {};
	collisionTestTempVel = motion.vel.vec;

	//TODO necessary to update it at here?
	// hitbox.updateHitbox(motion.trans);
}

void Game::RealEntity::postProcessCollisions_3(const UpdateTick delta) noexcept{
	for (const auto& data : collisionContext.postData | std::views::take(1)){
		calCollideTo(data.other, data.mainIntersection, 1/* / points.size()*/, delta);
		// for (const auto& intersection : data.intersections | std::views::take(1)){
		// 	calCollideTo(data.other, intersection, 1/* / points.size()*/, delta);
		// }
	}

	motion.trans.vec += collisionTestTempPos;

	//TODO do physics here
	// collisionContext.processIntersections(*this);

}

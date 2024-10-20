module Game.World.State;

import Game.World.RealEntity;

void Game::WorldState::draw(const Geom::OrthoRectFloat viewport) const{
	drawGroup(realEntities, viewport);
}

void Game::WorldState::updateQuadTree(){
	quadTree.reserved_clear();
	for(std::shared_ptr<RealEntity>& value : realEntities.entities | std::views::values){
		value->clearCollisionData();
		quadTree.insert(*value);
	}
}

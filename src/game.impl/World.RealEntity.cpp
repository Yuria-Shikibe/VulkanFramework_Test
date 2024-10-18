module Game.World.RealEntity;

import Game.World.State;

void Game::RealEntity::notifyDelete(WorldState& worldState) noexcept{
	Entity::notifyDelete(worldState);

	worldState.realEntities.notifyDelete(getID());
}

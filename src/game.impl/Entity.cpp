module Game.Entity;

import Game.World.State;

void Game::Entity::notifyDelete(WorldState& worldState) noexcept{
	state = EntityState::deletable;
	worldState.all.notifyDelete(getID());
}

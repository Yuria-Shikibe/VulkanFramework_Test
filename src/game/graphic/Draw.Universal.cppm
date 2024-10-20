module;

export module Game.Graphic.Draw.Universal;

export namespace Game{
	class Hitbox;
	class RealEntity;
}

export namespace Game::Graphic::Draw{
	void hitbox(const Hitbox& hitbox, float z);
	void realEntity(const RealEntity& entity);
}

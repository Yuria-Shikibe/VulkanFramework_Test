module Game.Graphic.Draw.Universal;


import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;
import Graphic.Renderer.World;
import Core.Global.Graphic;

import Game.World.RealEntity;
import Game.Entity.HitBox;


using FxParam = Graphic::InstantBatchAutoParam<Graphic::Vertex_World, Graphic::Draw::DepthModifier>;
using Drawer = Graphic::Draw::Drawer<FxParam::VertexType>;

namespace Colors = Graphic::Colors;

FxParam getParam(const float z){
	FxParam autoParam{Graphic::Effect::getBatch(), Graphic::Draw::WhiteRegion};
	autoParam.modifier.depth = z;
	return autoParam;
}

void Game::Graphic::Draw::hitbox(const Hitbox& hitbox, const float z){
	namespace Draw = ::Graphic::Draw;

	auto autoParam = getParam(z);

	for (const auto & component : hitbox.components){
		Drawer::Line::circularPoly_fixed<4>(autoParam, 4.f, component.box, Colors::WHITE);
		Drawer::Line::line(++autoParam, 4.f,
			hitbox.trans.vec, component.box.v0,
			Colors::ROYAL.copy().toLightColor(), Colors::YELLOW.copy().toLightColor());
	}
}

void Game::Graphic::Draw::realEntity(const RealEntity& entity){

	namespace Draw = ::Graphic::Draw;
	auto autoParam = getParam(entity.zLayer);

	for (const auto & component : entity.hitbox.components){
		Drawer::Line::circularPoly_fixed<4>(autoParam, 4.f, component.box, entity.manifold.underCorrection ? Colors::RED_DUSK : Colors::WHITE);
		Drawer::Line::line(++autoParam, 4.f,
			entity.hitbox.trans.vec, component.box.v0,
			Colors::ROYAL.copy().toLightColor(), Colors::YELLOW.copy().toLightColor());
	}

	for (const auto & data : entity.manifold.postData){
		Drawer::rectOrtho(++autoParam, data.mainIntersection.pos, 18, Colors::ROYAL.copy().toLightColor());
		Drawer::Line::lineAngle(++autoParam, 4.f, {data.mainIntersection.pos, data.mainIntersection.normal.angle()}, 90, Colors::CRIMSON.copy().toLightColor());
		Drawer::Line::line(++autoParam, 4.f, entity.motion.pos(), entity.motion.pos() + entity.last, Colors::ACID.copy().toLightColor());
	}
}

// void Game::         

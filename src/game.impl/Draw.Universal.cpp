module Game.Graphic.Draw.Universal;


import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;
import Graphic.Renderer.World;
import Core.Global.Graphic;
import Game.Entity.HitBox;

import Game.World.RealEntity;

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
			hitbox.trans.vec, component.box.transform.vec,
			Colors::ROYAL.copy().toLightColor(), Colors::YELLOW.copy().toLightColor());
	}
}

// void Game::         

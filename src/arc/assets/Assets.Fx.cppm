export module Assets.Fx;

import Graphic.Effect.Manager;
import Graphic.Effect.Derives;

import Graphic.Color;
import Graphic.Draw.Func;
import Graphic.Draw.Interface;
import Graphic.Vertex;

import Graphic.Batch.AutoDrawParam;

import Geom.Vector2D;
import Geom.Rect_Orthogonal;
import std;

namespace Assets::Fx{
	using FxParam = Graphic::InstantBatchAutoParam<Graphic::Vertex_World, Graphic::Draw::DepthModifier>;
	using Drawer = Graphic::Draw::Drawer<FxParam::VertexType>;

	FxParam getParam(const Graphic::Effect& effect){
		FxParam autoParam{Graphic::Effect::getBatch(), Graphic::Draw::WhiteRegion};
		autoParam.modifier.depth = effect.data.zLayer;
		return autoParam;
	}

	export
	constexpr Graphic::EffectDrawer_StaticFunc<[](const Graphic::Effect& effect){
		using namespace Graphic;

		auto autoParam = getParam(effect);
		const auto color = Colors::CLEAR.copy().appendLightColor(Colors::WHITE.createLerp(effect.data.color, effect.data.progress.getMargin(0.35f)));
		const auto stroke = effect.data.progress.getInv() * 4.5f;

		Drawer::Line::circle(autoParam, stroke, effect.pos(), effect.data.progress.get(Math::Interp::pow2Out) * effect.drawer->defClipRadius, color);
	}> CircleOut{50, 90.f};
}


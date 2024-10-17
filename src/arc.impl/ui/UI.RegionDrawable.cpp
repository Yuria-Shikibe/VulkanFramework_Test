module;

#include <cassert>

module Core.UI.RegionDrawable;

import Graphic.Renderer.UI;
import Graphic.Batch.Exclusive;
import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;
import Graphic.Draw.NinePatch;

import Graphic.Vertex;

void Core::UI::ImageRegionDrawable::draw(
	Graphic::RendererUI& renderer,
	const Geom::OrthoRectFloat rect,
	const Graphic::Color color) const{

	Graphic::Draw::Drawer<Graphic::Vertex_UI>::rectOrtho(Graphic::getParamOf<Graphic::Vertex_UI>(renderer.batch, region), rect, color);
}

void Core::UI::TextureNineRegionDrawable_Ref::draw(
	Graphic::RendererUI& renderer,
	const Geom::OrthoRectFloat rect,
	const Graphic::Color color) const{
	assert(region != nullptr);
	Graphic::InstantBatchAutoParam<Graphic::Vertex_UI> param{renderer.batch};

	Graphic::Draw::drawNinePatch(param, *region, rect, color);
}

void Core::UI::TextureNineRegionDrawable_Owner::draw(
	Graphic::RendererUI& renderer,
	const Geom::OrthoRectFloat rect,
	const Graphic::Color color) const{
	Graphic::InstantBatchAutoParam<Graphic::Vertex_UI> param{renderer.batch};

	Graphic::Draw::drawNinePatch(param, region, rect, color);
}

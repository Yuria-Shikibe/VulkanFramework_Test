module Core.UI.RegionDrawable;

import Graphic.Renderer.UI;
import Graphic.Batch.Exclusive;
import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;
import Graphic.Draw.NinePatch;

import Core.Vulkan.Vertex;

void Core::UI::ImageRegionDrawable::draw(
	Graphic::RendererUI& renderer,
	const Geom::OrthoRectFloat rect,
	const Graphic::Color color) const{

	Graphic::Draw::Drawer<Vulkan::Vertex_UI>::rectOrtho(Graphic::getParamOf<Vulkan::Vertex_UI>(renderer.batch, region), rect, color);
}

void Core::UI::TextureNineRegionDrawable_Ref::draw(
	Graphic::RendererUI& renderer,
	const Geom::OrthoRectFloat rect,
	const Graphic::Color color) const{
	Graphic::InstantBatchAutoParam<Vulkan::Vertex_UI> param{renderer.batch};

	Graphic::Draw::drawNinePatch(param, *region, rect, color);
}

void Core::UI::TextureNineRegionDrawable_Owner::draw(
	Graphic::RendererUI& renderer,
	const Geom::OrthoRectFloat rect,
	const Graphic::Color color) const{
	Graphic::InstantBatchAutoParam<Vulkan::Vertex_UI> param{renderer.batch};

	Graphic::Draw::drawNinePatch(param, region, rect, color);
}

module Core.UI.ScrollPanel;

import Graphic.Renderer.UI;
import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;
import Core.Vulkan.Vertex;

void Core::UI::ScrollPanel::drawMain() const{
	Group::drawMain();

	if(enableHoriScroll() || enableVertScroll())getScene()->renderer->pushScissor({Geom::FromExtent, absPos().addY(getBarSize().y), getViewportSize()}, true);
}

void Core::UI::ScrollPanel::drawPost() const{
	Group::drawPost();

	const bool enableHori = enableHoriScroll();
	const bool enableVert = enableVertScroll();

	using namespace Graphic;

	Draw::DrawContext context{};
	BatchAutoParam<Vulkan::Vertex_UI> ac{getScene()->renderer->batch, context.whiteRegion};

	auto param = ac.get();

	context.color = Colors::YELLOW;
	context.stroke = 2.f;
	context.color.a = 0.5f;
	if(enableHori || enableVert)getScene()->renderer->popScissor();

	if(enableHori){
		Draw::Drawer<Vulkan::Vertex_UI>::rectOrtho(++param, getHoriBarRect(), context.color);
	}

	if(enableVert){
		Draw::Drawer<Vulkan::Vertex_UI>::rectOrtho(++param, getVertBarRect(), context.color);
	}
}

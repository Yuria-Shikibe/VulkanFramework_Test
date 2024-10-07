module Core.UI.ScrollPanel;

import Core.UI.Scene;
import Graphic.Renderer.UI;
import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;
import Core.Vulkan.Vertex;

void Core::UI::ScrollPanel::drawMain() const{
	Group::drawMain();

	if(enableHoriScroll() || enableVertScroll())getRenderer().pushScissor({Geom::FromExtent, absPos().addY(getBarSize().y), getViewportSize()}, true);
}

void Core::UI::ScrollPanel::drawPost() const{
	Group::drawPost();

	const bool enableHori = enableHoriScroll();
	const bool enableVert = enableVertScroll();

	using namespace Graphic;

	InstantBatchAutoParam param{getBatch(), Draw::WhiteRegion};
	auto color = graphicProp().baseColor;
	color.a = 0.3f * graphicProp().getOpacity();

	if(enableHori || enableVert)getRenderer().popScissor();

	if(enableHori){
		Draw::Drawer<Vulkan::Vertex_UI>::rectOrtho(++param, getHoriBarRect(), color);
	}

	if(enableVert){
		Draw::Drawer<Vulkan::Vertex_UI>::rectOrtho(++param, getVertBarRect(), color);
	}
}

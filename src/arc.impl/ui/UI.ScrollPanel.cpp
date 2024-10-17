module Core.UI.ScrollPanel;

import Core.UI.Scene;
import Graphic.Renderer.UI;
import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;
import Graphic.Vertex;

void Core::UI::ScrollPanel::drawMain() const{
	Group::drawMain();

	if(enableHoriScroll() || enableVertScroll())getRenderer().pushScissor({Geom::FromExtent, absPos().addY(getBarSize().y).add(prop().boarder.bot_lft()), getViewportSize()}, true);
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
		Draw::Drawer<Vertex_UI>::rectOrtho(++param, getHoriBarRect(), color);
	}

	if(enableVert){
		Draw::Drawer<Vertex_UI>::rectOrtho(++param, getVertBarRect(), color);
	}
}

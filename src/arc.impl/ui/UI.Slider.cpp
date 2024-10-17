module Core.UI.Slider;

import Graphic.Renderer.UI;
import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;
import Graphic.Vertex;

void Core::UI::Slider::drawMain() const{
	using namespace Graphic;

	Element::drawMain();
	
	InstantBatchAutoParam param{getBatch(), Draw::WhiteRegion};

	Rect rect{};// = property.getValidBound_absolute();
	rect.setSize(getBarSize());

	rect.src = contentSrcPos() + getBarCurPos();
	Draw::Drawer<Vertex_UI>::rectOrtho(++param, rect, Colors::GRAY.copy().mulA(graphicProp().getOpacity()));

	rect.src = contentSrcPos() + getBarLastPos();
	Draw::Drawer<Vertex_UI>::rectOrtho(++param, rect, Colors::LIGHT_GRAY.copy().mulA(graphicProp().getOpacity()));
	//
	// Overlay::color(Colors::LIGHT_GRAY);
	// Overlay::alpha(0.15f + (isPressed() ? 0.1f : 0.0f));
	// Overlay::Fill::rectOrtho(Overlay::getContextTexture(), rect);
	// Overlay::alpha();
	//
	// Overlay::color(Colors::GRAY);
	// rect.setSize(getBarDrawSize());
	// rect.move(getBarCurPos());
	// Overlay::Fill::rectOrtho(Overlay::getContextTexture(), rect);
	//
	// Overlay::color(Colors::LIGHT_GRAY);
	// rect.setSrc(getAbsSrc() + getBorder().bot_lft() + getBarLastPos());
	// Overlay::Fill::rectOrtho(Overlay::getContextTexture(), rect);
}

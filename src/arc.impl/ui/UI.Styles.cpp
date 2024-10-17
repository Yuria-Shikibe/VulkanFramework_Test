module Core.UI.Styles;

import Core.UI.Element;

import Core.UI.Scene;
import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;
import Graphic.Draw.NinePatch;

Graphic::Color Core::UI::Style::Palette::onInstance(const Element& element) const{
	Graphic::Color color;
	if(element.getCursorState().pressed){
		color = onPress;
	}else if(element.getCursorState().focused){
		color = onFocus;
	}else{
		color = general;
	}

	return color.mulA(element.graphicProp().getOpacity());
}

void Core::UI::Style::RoundStyle::draw(const Element& element) const{
	using namespace Graphic;

	InstantBatchAutoParam<Vertex_UI> param{element.getBatch()};

	const auto rect = element.prop().getBound_absolute();

	Draw::drawNinePatch(param, base, rect, base.palette.onInstance(element));

	Draw::drawNinePatch(param, edge, rect, edge.palette.onInstance(element));

#if DEBUG_CHECK
	// param << Draw::WhiteRegion;
	// Draw::Drawer<Vulkan::Vertex_UI>::Line::rectOrtho(++param, 1.f, element.prop().getValidBound_absolute(), Graphic::Colors::PINK);
#endif
}

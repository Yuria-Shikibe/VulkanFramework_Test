module Core.UI.TextElement;

import Font.TypeSettings.Renderer;
import Graphic.Renderer.UI;
import Graphic.Batch.Exclusive;

void Core::UI::TextElement::drawMain() const{
	Element::drawMain();

	Font::TypeSettings::draw(getBatch(), glyphLayout, getTextOffset());

}

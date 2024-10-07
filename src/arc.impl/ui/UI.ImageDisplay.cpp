module Core.UI.ImageDisplay;


void Core::UI::ImageDisplay::drawMain() const{
	Element::drawMain();

	if(drawable){
		const auto size = Align::embedTo(scaling, drawable->getDefSize(), getValidSize());
		const auto offset = Align::getOffsetOf(imageAlign, size, property.getValidBound_absolute());

		drawable->draw(getRenderer(), Rect{Geom::FromExtent, offset, size}, graphicProp().getScaledColor());
	}
}

module Core.UI.Cell;

import Core.UI.Element;

void Core::UI::PassiveCell::applyBoundToElement(Element* element) const noexcept{
	const Geom::Vec2 size = allocatedBound.getSize() * sizeScale.getSize() * roomScale;
	element->resize_unchecked((size - margin.getSize()));
}

void Core::UI::PassiveCell::applyPosToElement(Element* element, const Geom::Vec2 absSrc) const{
	const Geom::Vec2 offset =
		Align::getOffsetOf(scaleAlign, element->getSize(), allocatedBound) +
		// Align::getTransformedOffsetOf(scaleAlign, sizeScale.getSrc() * roomScale * allocatedBound.getSize()) +
		Align::getOffsetOf(scaleAlign, margin);


	element->prop().relativeSrc = offset;

	element->updateAbsSrc(absSrc);
	element->notifyLayoutChanged(SpreadDirection::lower);
}

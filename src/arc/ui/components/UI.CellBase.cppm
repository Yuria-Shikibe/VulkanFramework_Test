//
// Created by Matrix on 2024/9/14.
//

export module Core.UI.CellBase;

import Geom.Rect_Orthogonal;

export namespace Core::UI{
	struct CellBase{
		Geom::Rect_Orthogonal<float> allocatedBound{};
		float roomScale{1.f};
		float contentScale{1.f};
	};
}

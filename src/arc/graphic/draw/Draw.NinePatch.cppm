//
// Created by Matrix on 2024/9/28.
//

export module Graphic.Draw.NinePatch;

export import Graphic.ImageNineRegion;
export import Graphic.Draw.Func;

import std;
import ext.guard;

export namespace Graphic::Draw{

	template </*bool guardOriginalImage = false, */AutoAcquirableParam Param = EmptyParam>
		requires (ImageMutableParam<Param>)
	void drawNinePatch(Param& param, const ImageViewNineRegion& nineRegion, const Geom::OrthoRectFloat bound, const Color color){
		const auto rects = nineRegion.getPatches(bound);

		if constexpr (false){
			ext::resumer guard1{param.uv};
			ext::resumer guard2{param.index};

			for(std::size_t i = 0; i < ImageViewNineRegion::size; ++i){
				param << nineRegion.imageView.view;
				param << nineRegion.regions[i];
				Drawer<typename Param::VertexType>::rectOrtho(++param, rects[i], color);
			}
		}else{
			for(std::size_t i = 0; i < ImageViewNineRegion::size; ++i){
				param << nineRegion.imageView.view;
				param << nineRegion.regions[i];
				Drawer<typename Param::VertexType>::rectOrtho(++param, rects[i], color);
			}
		}

	}
}

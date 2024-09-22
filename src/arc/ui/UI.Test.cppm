//
// Created by Matrix on 2024/9/21.
//

export module Core.UI.Test;

import Core.UI.UniversalGroup;

import Graphic.Batch.AutoDrawParam;
import Graphic.Draw.Func;
import Core.Vulkan.Vertex;

import Graphic.Renderer.UI;

import std;

export namespace Core::UI::Test{
	template <std::derived_from<CellBase> T, std::derived_from<CellAdaptor<T>> Adaptor = CellAdaptor<T>>
	void drawCells(const UniversalGroup<T, Adaptor>& element){
		using namespace Graphic;

		InstantBatchAutoParam param{static_cast<RendererUI*>(element.getScene()->renderer)->batch, Draw::WhiteRegion};

		for (const auto& cell : element.getCells()){
			Draw::Drawer<Vulkan::Vertex_UI>::Line::rectOrtho(
				param, 1.f, cell.cell.allocatedBound.copy().move(element.absPos()), Colors::RED);
		}
	}
}

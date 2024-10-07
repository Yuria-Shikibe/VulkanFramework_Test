//
// Created by Matrix on 2024/8/27.
//

export module Font.TypeSettings.Renderer;

export import Font.TypeSettings;

import Geom.Vector2D;

import Graphic.BatchData;
import Graphic.Batch.MultiThread;
import Graphic.Batch.AutoDrawParam;
import std;

//TEMP
import Graphic.Renderer.UI;
import Graphic.Draw.Func;
import Core.Vulkan.Vertex;


export namespace Font::TypeSettings{

	void draw(Graphic::Batch_Exclusive& batch, const std::shared_ptr<GlyphLayout>& layout, const Geom::Vec2 offset, float opacityScl = 1.f){
		using namespace Graphic;

		// Draw::DrawContext context{};
		// Graphic::BatchAutoParam_Exclusive<Core::Vulkan::Vertex_UI> ac{batch, context.whiteRegion};
		// auto param = ac.get();
		//
		// context.color = Graphic::Colors::WHITE;
		// context.color.a = 0.45f;


		Graphic::Color tempColor{};

		for (const auto& row : layout->elements){
			const auto lineOff = row.src + offset;
			// Draw::Drawer<Core::Vulkan::Vertex_UI>::Line::rectOrtho(param, context.stroke, row.getRectBound().move(offset), context.color);

			for (auto && glyph : row.glyphs){
				if(!glyph.glyph->view)continue;
				auto [imageIndex, sz, dataPtr] = batch.acquire(glyph.glyph->view, 1);

				tempColor = glyph.fontColor;

				if(opacityScl != 1.f){
					tempColor.a *= opacityScl;
				}

				new(dataPtr) std::array{
					Core::Vulkan::Vertex_UI{glyph.v00().add(lineOff), {imageIndex}, tempColor, glyph.glyph->v01},
					Core::Vulkan::Vertex_UI{glyph.v10().add(lineOff), {imageIndex}, tempColor, glyph.glyph->v11},
					Core::Vulkan::Vertex_UI{glyph.v11().add(lineOff), {imageIndex}, tempColor, glyph.glyph->v10},
					Core::Vulkan::Vertex_UI{glyph.v01().add(lineOff), {imageIndex}, tempColor, glyph.glyph->v00},
				};

				// Draw::Drawer<Core::Vulkan::Vertex_UI>::Line::rectOrtho(param, context.stroke, Geom::OrthoRectFloat{glyph.src, glyph.end}.move(lineOff), context.color);

			}

		}

		// context.color = Graphic::Colors::PALE_GREEN;

		// Draw::Drawer<Core::Vulkan::Vertex_UI>::Line::rectOrtho(param, context.stroke, Geom::OrthoRectFloat{layout->size}.move(offset), context.color);

	}
}

//
// Created by Matrix on 2024/8/27.
//

export module Font.TypeSettings.Renderer;

export import Font.TypeSettings;

import Geom.Vector2D;

import Graphic.Batch;
import std;

//TEMP
import Graphic.RendererUI;
import Graphic.Draw.Func;
import Core.Vulkan.Vertex;


export namespace Font::TypeSettings{
	template <typename T = std::identity>
	struct AutoDrawParam : Graphic::Draw::DrawParam<T>{
		Graphic::Batch& batch;
		std::shared_lock<std::shared_mutex> lk{};
		const Graphic::ImageViewRegion* region{};

		[[nodiscard]] explicit AutoDrawParam(Graphic::Batch& batch, const Graphic::ImageViewRegion* region = nullptr)
			: batch{batch}, region{region}{}

		void setRegion(const Graphic::ImageViewRegion* region){
			this->region = region;
		}

		AutoDrawParam& operator++(){
			return acquire();
		}

		AutoDrawParam& acquire(){
			auto [imageIndex, dataPtr, captureLock] = batch.acquire(region->imageView);
			this->index = {imageIndex};
			this->uv = region;
			this->dataPtr = dataPtr;
			lk = std::move(captureLock);

			return *this;
		}
	};

	void draw(Graphic::Batch& batch, const std::shared_ptr<Layout>& layout, const Geom::Vec2 offset){
		using namespace Graphic;

		Draw::DrawContext context{};
		AutoDrawParam param{batch, context.whiteRegion};


		// context.color = Graphic::Colors::BLACK;
		// Draw::Drawer<Core::Vulkan::Vertex_UI>::rectOrtho(++param, Geom::OrthoRectFloat{layout->size}.move(offset), context.color);
		context.color = Graphic::Colors::WHITE;


		context.color.a = 0.45f;
		for (const auto& row : layout->elements){
			const auto lineOff = row.src + offset;
			Draw::Drawer<Core::Vulkan::Vertex_UI>::Line::rectOrtho(param, context.stroke, row.getRectBound().move(offset), context.color);

			for (auto && glyph : row.glyphs){
				auto [imageIndex, dataPtr, captureLock] = batch.acquire(glyph.glyph->imageView);

				new(dataPtr) std::array{
					Core::Vulkan::Vertex_UI{glyph.v00().add(lineOff), {imageIndex}, glyph.fontColor, glyph.glyph->v01},
					Core::Vulkan::Vertex_UI{glyph.v10().add(lineOff), {imageIndex}, glyph.fontColor, glyph.glyph->v11},
					Core::Vulkan::Vertex_UI{glyph.v11().add(lineOff), {imageIndex}, glyph.fontColor, glyph.glyph->v10},
					Core::Vulkan::Vertex_UI{glyph.v01().add(lineOff), {imageIndex}, glyph.fontColor, glyph.glyph->v00},
				};

				Draw::Drawer<Core::Vulkan::Vertex_UI>::Line::rectOrtho(param, context.stroke, Geom::OrthoRectFloat{glyph.src, glyph.end}.move(lineOff), context.color);

			}
			
		}

		context.color = Graphic::Colors::PALE_GREEN;

		Draw::Drawer<Core::Vulkan::Vertex_UI>::Line::rectOrtho(param, context.stroke, Geom::OrthoRectFloat{layout->size}.move(offset), context.color);
	}
}

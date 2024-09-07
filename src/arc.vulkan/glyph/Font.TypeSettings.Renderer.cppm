//
// Created by Matrix on 2024/8/27.
//

export module Font.TypeSettings.Renderer;

export import Font.TypeSettings;

import Geom.Vector2D;

import Graphic.Batch;
import Core.Vulkan.BatchData;
import Graphic.Batch2;
import std;

//TEMP
import Graphic.Renderer.UI;
import Graphic.Draw.Func;
import Core.Vulkan.Vertex;


export namespace Font::TypeSettings{
	template <typename T = std::identity>
	struct AutoDrawParamAc : Graphic::AutoDrawSpaceAcquirer<Core::Vulkan::Vertex_UI, AutoDrawParamAc<T>>{
		Graphic::Batch2& batch;
		const Graphic::ImageViewRegion* region{};

		[[nodiscard]] explicit AutoDrawParamAc(Graphic::Batch2& batch, const Graphic::ImageViewRegion* region = nullptr)
			: batch{batch}, region{region}{}

		void setRegion(const Graphic::ImageViewRegion* region){
			this->region = region;
		}

		void reserve(const std::size_t count){
			this->append(batch.acquireOnce(region->imageView, count));
		}

		struct Acquirer : Graphic::AutoDrawSpaceAcquirer<Core::Vulkan::Vertex_UI, AutoDrawParamAc<T>>::BasicAcquirer, Graphic::Draw::DrawParam<T>{
			using Graphic::AutoDrawSpaceAcquirer<Core::Vulkan::Vertex_UI, AutoDrawParamAc<T>>::BasicAcquirer::BasicAcquirer;

			Acquirer& operator++(){
				auto rst = (this->next());
				this->dataPtr = rst.first;
				this->index.textureIndex = rst.second;

				return *this;
			}
		};

		Acquirer get(){
			Acquirer acquirer{*this};
			acquirer.uv = region;
			return acquirer;
		}
	};

	void draw(Graphic::Batch2& batch, const std::shared_ptr<Layout>& layout, const Geom::Vec2 offset){
		using namespace Graphic;

		Draw::DrawContext context{};
		AutoDrawParamAc ac{batch, context.whiteRegion};
		auto param = ac.get();

		context.color = Graphic::Colors::WHITE;

		context.color.a = 0.45f;
		for (const auto& row : layout->elements){
			const auto lineOff = row.src + offset;
			Draw::Drawer<Core::Vulkan::Vertex_UI>::Line::rectOrtho(param, context.stroke, row.getRectBound().move(offset), context.color);

			for (auto && glyph : row.glyphs){
				if(!glyph.glyph->imageView)continue;
				auto [imageIndex, sz, dataPtr, captureLock] = batch.acquire(glyph.glyph->imageView, 1);

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

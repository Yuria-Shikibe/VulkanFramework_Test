//
// Created by Matrix on 2024/9/11.
//

export module Core.Global.Graphic;

export import Graphic.Effect.Manager;
export import Graphic.Renderer.World;
export import Graphic.Batch.MultiThread;

export namespace Core::Global{
	Graphic::EffectManager mainEffectManager{};

	Graphic::RendererWorld* rendererWorld{};
}
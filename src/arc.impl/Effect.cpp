module Graphic.Effect;

import Graphic.Effect.Manager;
import Graphic.Camera2D;
import Core.Global.Graphic;

Geom::OrthoRectFloat Graphic::EffectDrawer::getClipBound(const Effect& effect) const noexcept{
	if(std::isnan(defClipRadius)){
		return getClipBoundVirtual(effect);
	}else{
		return Geom::OrthoRectFloat{effect.pos(), defClipRadius, defClipRadius};
	}
}

Geom::OrthoRectFloat Graphic::EffectDrawer::getClipBoundVirtual(const Effect& effect) const noexcept{
	return {effect.data.trans.vec, defClipRadius};
}

Graphic::Effect& Graphic::EffectDrawer::launch(const EffectBasicData& data, EffectManager& manager) const{
	return manager.acquire().setData(data).setDrawer(this);
}

Graphic::Effect& Graphic::EffectDrawer::launch(const EffectBasicData& data) const{
	return launch(data, getDefManager());
}

extern Graphic::EffectManager& Graphic::getDefManager(){
	return Core::Global::mainEffectManager;
}

Graphic::Batch_Exclusive& Graphic::Effect::getBatch() noexcept{
	return Core::Global::rendererWorld->batch;
}

//
// Graphic::Effect* Graphic::EffectShake::create(EffectManager* manager, Core::Camera2D* camera, const Geom::Vec2 pos, const float intensity, float fadeSpeed) const{
// 	auto* eff = suspendOn(manager);
// 	fadeSpeed = fadeSpeed > 0 ? fadeSpeed : intensity / 32.f;
// 	eff->set({pos, intensity}, Graphic::Colors::CLEAR, intensity / fadeSpeed, camera);
// 	eff->zLayer = fadeSpeed;
//
// 	return eff;
// }
//
// Graphic::Effect* Graphic::EffectShake::create(const Geom::Vec2 pos, const float intensity, const float fadeSpeed) const{
// 	return create(getDefManager(), Core::camera.get(), pos, intensity, fadeSpeed);
// }
//
// void Graphic::EffectShake::operator()(Effect& effect) const{
// 	if(auto* camera = std::any_cast<Core::Camera2D*>(effect.additionalData)){
// 		const float dst = effect.trans.vec.dst(camera->getPosition());
// 		camera->shake(getIntensity(dst) * effect.trans.rot, effect.zLayer);
// 	}
// }

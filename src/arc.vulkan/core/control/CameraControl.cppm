//
// Created by Matrix on 2024/5/10.
//

export module Core.Ctrl:CameraControl;

export import Graphic.Camera2D;
export import Geom.Vector2D;
import :FocusInterface;
// import ext.Guard;

export namespace Core::Ctrl{
	struct CameraFocus : FocusData<Graphic::Camera2D*>{
		// using Guard = ext::Guard<CameraControl2D, &FocusData<Graphic::Camera2D*>::current>;

		void switchFocus(Graphic::Camera2D* focus){
			this->current = focus;
		}

		void switchDef(){
			this->current = fallback;
		}

		void move(const Geom::Vec2 movement) const{
			if(current)current->move(movement);
		}

		void set(const Geom::Vec2 pos) const{
			if(current)current->setPosition(pos);
		}

		auto operator->() const{
			return current;
		}

		explicit operator bool() const{
			return current != nullptr;
		}
	};
}

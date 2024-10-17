//
// Created by Matrix on 2024/5/10.
//

export module Assets.Ctrl;

export import Core.Input;
export import Core.Global;
// export import Core.MainLoopManager;
// import UI.Screen;
export import Core.Ctrl.Constants;
export import OS.Ctrl.Operation;
export import Core.Ctrl.Bind;
export import Core.Global.Focus;

import Core.UI.Root;
import Core.Global.UI;

import std;

import ext.heterogeneous;

namespace Assets::Ctrl{
	float baseCameraMoveSpeed = 60;
	float fastCameraMoveSpeed = 300;
	bool disableMove = false;
	float cameraMoveSpeed = baseCameraMoveSpeed;

    namespace CC = Core::Ctrl;

	CC::Operation cameraMoveLeft{"cmove-left", CC::KeyBind(CC::Key::A, CC::Act::Continuous, +[]{
		if(!disableMove)Core::Global::Focus::camera.move({-cameraMoveSpeed * Core::Global::timer.globalDeltaTick(), 0});
	})};

	CC::Operation cameraMoveRight{"cmove-right", CC::KeyBind(CC::Key::D, CC::Act::Continuous, +[]{
		if(!disableMove)Core::Global::Focus::camera.move({cameraMoveSpeed * Core::Global::timer.globalDeltaTick(), 0});
	})};

	CC::Operation cameraMoveUp{"cmove-up", CC::KeyBind(CC::Key::W, CC::Act::Continuous, +[]{
		if(!disableMove)Core::Global::Focus::camera.move({0,  cameraMoveSpeed * Core::Global::timer.globalDeltaTick()});
	})};

	CC::Operation cameraMoveDown{"cmove-down", CC::KeyBind(CC::Key::S, CC::Act::Continuous, +[] {
		if(!disableMove)Core::Global::Focus::camera.move({0, -cameraMoveSpeed * Core::Global::timer.globalDeltaTick()});
	})};

	// CC::Operation cameraTeleport{"cmove-telp", CC::KeyBind(CC::Mouse::LMB, CC::Act::DoubleClick, +[] {
	// 	if(Core::ui->cursorCaptured()){
	//
	// 	}else{
	// 		Core::camera->setPosition(Core::Util::getMouseToWorld());
	// 	}
	// })};

	CC::Operation cameraLock{"cmove-lock", CC::KeyBind(CC::Key::M, CC::Act::Press, +[] {
		disableMove = !disableMove;
	})};

	CC::Operation boostCamera_Release{"cmrboost-rls", CC::KeyBind(CC::Key::Left_Shift, CC::Act::Release, +[] {
		cameraMoveSpeed = baseCameraMoveSpeed;
	})};

	CC::Operation boostCamera_Press{"cmrboost-prs", CC::KeyBind(CC::Key::Left_Shift, CC::Act::Press, +[] {
		cameraMoveSpeed = fastCameraMoveSpeed;
	}), {boostCamera_Release.name}};
	//
	CC::Operation pause{"pause", CC::KeyBind(CC::Key::P, CC::Act::Press, +[] {
		// std::cout << "pressed" << std::endl;
		Core::Global::timer.setPause(!Core::Global::timer.isPaused());
	})};

	// CC::Operation hideUI{"ui-hide", CC::KeyBind(CC::Key::H, CC::Act::Press, +[] {
	// 	if(Core::ui->isHidden){
	// 		Core::ui->show();
	// 	} else{
	// 		if(!Core::ui->hasTextFocus())Core::ui->hide();
	// 	}
	// })};

	// CC::Operation shoot{"shoot", CC::KeyBind(CC::Key::F, CC::Act::Continuous, +[] {

	// })};

	export{
		CC::KeyMapping* mainControlGroup{};

		CC::OperationGroup basicGroup{
				"basic-group", {
					CC::Operation{cameraMoveLeft},
					CC::Operation{cameraMoveRight},
					CC::Operation{cameraMoveUp},
					CC::Operation{cameraMoveDown},
					CC::Operation{boostCamera_Press},
					CC::Operation{boostCamera_Release},

					CC::Operation{cameraLock},
					// CC::Operation{cameraTeleport},

					CC::Operation{pause},
					// CC::Operation{hideUI},
				}
			};

		CC::OperationGroup gameGroup{"game-group"};

		ext::string_hash_map<CC::OperationGroup*> allGroups{
			{basicGroup.getName(), &basicGroup},
			{gameGroup.getName(), &gameGroup}
		};

		ext::string_hash_map<CC::KeyMapping*> relatives{};

		void apply(){
			mainControlGroup->clear();

			for (auto group : allGroups | std::ranges::views::values){
				for (const auto & bind : group->getBinds() | std::ranges::views::values){
					mainControlGroup->registerBind(CC::InputBind{bind.customeBind});
				}
			}
		}

        void load() {
            mainControlGroup = &Core::Global::input->registerSubInput("game");
            basicGroup.targetGroup = mainControlGroup;
            apply();

			using namespace Core::Global;

            input->scrollListeners.emplace_back([](const float x, const float y) -> void {
            	if(!UI::root->focusIdle()){
            		UI::root->inputScroll(x, y);
            	}else if(Focus::camera){
            		Focus::camera->setTargetScale(Focus::camera->getTargetScale() + y * 0.05f);
            	}
            });
        }
	}
}

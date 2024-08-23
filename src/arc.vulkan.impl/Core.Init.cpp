module Core.InitAndTerminate;

import Assets.Ctrl;
import Assets.Graphic;
import Assets.Directories;
import Core.Global.Focus;

import Core.FileTree;

import std;

void Core::bindFocus() {
    Focus::camera = Ctrl::CameraFocus{mainCamera, mainCamera};
}

void Core::initFileSystem() {
#if defined(ASSETS_DIR)
    const File dir{ASSETS_DIR};
#else
    const File dir{std::filesystem::current_path()};
#endif

    std::cout << "[Assets] Targeted Resource Root:" << dir.absolutePath() << std::endl;
    rootFileTree = Core::FileTree{dir};

    Assets::loadDir();
}

void Core::bindCtrlCommands() {
    Assets::Ctrl::load();
}

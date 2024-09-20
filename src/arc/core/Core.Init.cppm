export module Core.InitAndTerminate;

namespace Core::Global{
    export void initFileSystem();

    void initCtrl();

	void loadEXT();

    export void init();

	void initUI();

	export void postInit();

	export void terminate();
}

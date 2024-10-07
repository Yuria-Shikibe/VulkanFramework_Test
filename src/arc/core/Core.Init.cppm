export module Core.InitAndTerminate;

namespace Core::Global{
    export void initFileSystem();

    void initCtrl();

	void loadEXT();

    export void init_context();

	void initUI();

	export void init_assetsAndRenderers();

	export void terminate();
}

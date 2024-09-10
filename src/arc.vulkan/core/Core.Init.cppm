export module Core.InitAndTerminate;

export namespace Core{
    void initFileSystem();

    void bindFocus();

    void bindCtrlCommands();

	void loadEXT();

    void init();

	void postInit();

	void terminate();
}

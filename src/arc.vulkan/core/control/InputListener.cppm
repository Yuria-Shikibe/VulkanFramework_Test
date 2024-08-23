export module Core.InputListener;

export namespace Core::Ctrl{
	class InputListener {
	public:
		virtual ~InputListener() = default;

		virtual void inform(int key, int action, int mods) = 0;
	};
}
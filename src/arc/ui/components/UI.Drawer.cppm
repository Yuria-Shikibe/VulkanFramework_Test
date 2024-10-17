//
// Created by Matrix on 2024/10/7.
//

export module Core.UI.Drawer;

export namespace Core::UI{
	template <typename T>
	struct StyleDrawer{
		virtual ~StyleDrawer() = default;

		virtual void draw(const T& element) const = 0;
	};
}

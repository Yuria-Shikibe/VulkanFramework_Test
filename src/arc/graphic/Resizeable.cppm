export module Graphic.Resizeable;

import std;

export namespace Graphic{
	template <typename T>
        requires (std::is_arithmetic_v<T>)
	struct Resizeable
	{
		virtual ~Resizeable() = default;

		virtual void resize(T w, T h) = 0;
	};

	using ResizeableUInt = Resizeable<unsigned int>;
	using ResizeableInt = Resizeable<int>;
}



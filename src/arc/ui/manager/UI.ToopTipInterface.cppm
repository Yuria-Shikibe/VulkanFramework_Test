//
// Created by Matrix on 2024/10/1.
//

export module Core.UI.ToolTipInterface;

import Core.UI.ElementUniquePtr;
import Geom.Vector2D;
import Align;

import std;

export namespace Core::UI{
	enum struct ToolTipFollow{
		none,
		cursor,
		// parent
	};

	struct ToolTipAlignPos{
		std::variant<Geom::Vec2, ToolTipFollow> pos{};
		Align::Pos align{};
	};

	struct ToolTipOwner{
	protected:
		// static constexpr auto CursorPos = Geom::SNAN2;

		~ToolTipOwner() = default;

	public:

		/**
		 * @brief nullptr for default (scene.root)
		 * @return given parent
		 */
		[[nodiscard]] virtual Group* specifiedParent() const{
			return nullptr;
		}
		/**
		 * @brief
		 * @return the align reference point in screen space
		 */
		[[nodiscard]] virtual ToolTipAlignPos alignPolicy() const = 0;

		[[nodiscard]] virtual bool shouldDrop(Geom::Vec2 cursorPos) const = 0;

		[[nodiscard]] virtual bool shouldBuild(Geom::Vec2 cursorPos) const = 0;

		virtual void notifyDrop(){}

		[[nodiscard]] virtual ElementUniquePtr build(Scene& scene) = 0;

	protected:
		[[nodiscard]] Group* getToolTipTrueParent(Scene& scene) const;
	};

	template<typename T>
	struct FuncToolTipOwner : public ToolTipOwner{
	protected:
		~FuncToolTipOwner() = default;

		std::move_only_function<ElementUniquePtr(T&, Scene&)> toolTipBuilder{};
	public:
		template<ElemInitFunc InitFunc>
			requires std::invocable<InitFunc, T&, typename ElemInitFuncTraits<InitFunc>::ElemType&>
		decltype(auto) setToolTipBuilder(InitFunc&& initFunc){
			return std::exchange(toolTipBuilder, decltype(toolTipBuilder){[func = std::forward<InitFunc>(initFunc)](T& owner, Scene& scene){
				return ElementUniquePtr{static_cast<FuncToolTipOwner&>(owner).getToolTipTrueParent(), &scene, std::bind_front(func, std::ref(owner))};
			}});
		}

		ElementUniquePtr build(Scene& scene) override{
			return std::invoke(toolTipBuilder, static_cast<T&>(*this), scene);
		}
	};
}

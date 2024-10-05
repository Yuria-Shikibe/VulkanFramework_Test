//
// Created by Matrix on 2024/10/1.
//

export module Core.UI.ToolTipInterface;

import Core.UI.ElementUniquePtr;
import Geom.Vector2D;
import Align;

import std;

export namespace Core::UI{
	enum struct TooltipFollow{
		none,
		cursor,
		owner
	};

	struct TooltipAlignPos{
		std::variant<Geom::Vec2, TooltipFollow> pos{};
		Align::Pos align{};
	};

	struct TooltipOwner{
	protected:
		// static constexpr auto CursorPos = Geom::SNAN2;

		~TooltipOwner() = default;

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
		[[nodiscard]] virtual TooltipAlignPos getAlignPolicy() const = 0;

		[[nodiscard]] virtual bool tooltipShouldDrop(Geom::Vec2 cursorPos) const = 0;

		[[nodiscard]] virtual bool tooltipShouldBuild(Geom::Vec2 cursorPos) const = 0;

		virtual void notifyDrop(){
			std::println(std::cout, "dropped");
			std::cout.flush();
		}

		[[nodiscard]] virtual ElementUniquePtr build(Scene& scene) = 0;

	protected:
		[[nodiscard]] Group* getToolTipTrueParent(Scene& scene) const;
	};

	template<typename T>
	struct FuncToolTipOwner : public TooltipOwner{
	protected:
		~FuncToolTipOwner() = default;

		std::move_only_function<ElementUniquePtr(T&, Scene&)> toolTipBuilder{};
	public:
		template<ElemInitFunc InitFunc>
			requires std::invocable<InitFunc, T&, typename ElemInitFuncTraits<InitFunc>::ElemType&>
		decltype(auto) setTooltipBuilder(InitFunc&& initFunc){
			return std::exchange(toolTipBuilder, decltype(toolTipBuilder){[func = std::forward<InitFunc>(initFunc)](T& owner, Scene& scene){
				return ElementUniquePtr{
					static_cast<FuncToolTipOwner&>(owner).getToolTipTrueParent(scene),
					&scene,
					[&](typename ElemInitFuncTraits<InitFunc>::ElemType& e){
						std::invoke(func, owner, e);
				}};
			}});
		}

		template<ElemInitFunc InitFunc>
			requires std::invocable<InitFunc, typename ElemInitFuncTraits<InitFunc>::ElemType&>
		decltype(auto) setTooltipBuilder(InitFunc&& initFunc){
			return std::exchange(toolTipBuilder, decltype(toolTipBuilder){[func = std::forward<InitFunc>(initFunc)](T& owner, Scene& scene){
				return ElementUniquePtr{static_cast<FuncToolTipOwner&>(owner).getToolTipTrueParent(scene), &scene, func};
			}});
		}

		ElementUniquePtr build(Scene& scene) override{
			return std::invoke(toolTipBuilder, static_cast<T&>(*this), scene);
		}
	};

	struct ToolTipProperty{
		static constexpr float DisableAutoTooltip = -1.0f;
		static constexpr float DefTooltipHoverTime = 25.0f;

		TooltipFollow followTarget{TooltipFollow::none};

		Align::Pos followTargetAlign{Align::Pos::bottom_left};
		Align::Pos tooltipSrcAlign{Align::Pos::top_left};
		bool useStagnateTime{false};
		bool autoRelease{true};

		float minHoverTime{DefTooltipHoverTime};
		Geom::Vec2 offset{};

		[[nodiscard]] bool autoBuild() const noexcept{
			return minHoverTime >= 0.0f;
		}
	};

	template<typename T>
	struct StatedToolTipOwner : public FuncToolTipOwner<T>{
	protected:
		~StatedToolTipOwner() = default;

		ToolTipProperty tooltipProp{};

	public:
		template<ElemInitFunc InitFunc>
		void setTooltipState(const ToolTipProperty& toolTipProperty, InitFunc&& initFunc) noexcept{
			this->tooltipProp = toolTipProperty;
			this->setTooltipBuilder(std::forward<InitFunc>(initFunc));
		}

	};

}

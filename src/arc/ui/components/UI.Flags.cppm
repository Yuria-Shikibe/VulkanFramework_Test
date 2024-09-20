module;

#include "../src/ext/enum_operator_gen.hpp"

export module Core.UI.Flags;

import std;

export namespace Core::UI{
	enum struct SpaceAcquireType : std::uint8_t {
		ignored = 0,

		passiveX = 1u << 0u,
		passiveY = 1u << 1u,

		staticX = 1u << 2u,
		staticY = 1u << 3u,

		/**
		 * @brief Specify width is generated from height (Apply To X)
		 */
		ratioedToX = 1u << 4u,

		/**
		 * @brief Specify height is generated from width (Apply To Y)
		 */
		ratioedToY = 1u << 5u,



		staticXY = staticX | staticY,
		passiveXY = passiveX | passiveY,
	};

	enum struct ElementState{
		normal = 0u,
		invisible = 1u << 0u,
		activated = 1u << 1u,
		disabled = 1u << 2u,
		sleeping = 1u << 3u,
	};

	enum struct SpreadDirection : std::uint8_t {
		none = 0,
		local = 1u << 0,
		super = 1u << 1,
		child = 1u << 2,

		all = local | super | child,

		lower = local | child,
		upper = local | super,
	};

	BITMASK_OPS_BOOL(export, Core::UI::SpreadDirection)
	BITMASK_OPS_ADDITIONAL(export, Core::UI::SpreadDirection)
}

export namespace Core::UI{
	enum struct Interactivity : std::uint8_t {
		disabled,
		childrenOnly,
		enabled
	};

	bool operator&(const SpaceAcquireType a, const SpaceAcquireType b){
		return std::to_underlying(a) & std::to_underlying(b);
	}

	struct LayoutState{
		/**
		 * @brief Represents how the space should be allocated for an element
		 */
		SpaceAcquireType acquireType{};

		/**
		 * @brief Describes the accept direction in layout context
		 * e.g. An element in @link BedFace @endlink only accept layout notification from parent
		 */
		SpreadDirection acceptMask_context{SpreadDirection::all};

		/**
		 * @brief Describes the accept direction an element inherently owns
		 * e.g. An element of @link ScrollPanel @endlink deny children layout notify
		 */
		SpreadDirection acceptMask_inherent{SpreadDirection::all};


		/**
		 * @brief Determine whether this element is restricted in a cell, then disable up scale
		 */
		bool restrictedByParent{false};

	private:
		bool childrenChanged{};
		bool parentChanged{};
		bool changed{};

	public:
		[[nodiscard]] constexpr bool isRestricted() const noexcept{
			return restrictedByParent;
		}

		constexpr void ignoreChildren() noexcept{
			acceptMask_inherent -= SpreadDirection::child;
		}

		[[nodiscard]] constexpr bool isChildrenChanged() const noexcept{
			return childrenChanged;
		}

		[[nodiscard]] constexpr bool isParentChanged() const noexcept{
			return parentChanged;
		}

		[[nodiscard]] constexpr bool isChanged() const noexcept{
			return changed;
		}

		constexpr void setSelfChanged() noexcept{
			if(acceptMask_context & SpreadDirection::local && acceptMask_inherent & SpreadDirection::local){
				changed = true;
			}
		}

		constexpr bool notifyFromChildren() noexcept{
			if(acceptMask_context & SpreadDirection::child && acceptMask_inherent & SpreadDirection::child){
				childrenChanged = true;
				return true;
			}
			return false;
		}

		constexpr bool notifyFromParent() noexcept{
			if(acceptMask_context & SpreadDirection::super && acceptMask_inherent & SpreadDirection::super){
				parentChanged = true;
				return false;
			}
			return false;
		}

		constexpr bool checkChanged() noexcept{
			return std::exchange(changed, false);
		}

		constexpr void clear() noexcept{
			changed = childrenChanged = parentChanged = false;
		}
	};
}
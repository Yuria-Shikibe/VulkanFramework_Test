module;

#include <cassert>

export module Core.UI.ElementUniquePtr;

import std;
import ext.owner;
import ext.meta_programming;


export namespace Core::UI{
	struct Element;
	struct Scene;
	struct Group;

	template <typename Fn>
	concept ElemInitFunc = requires{
		requires ext::is_complete<ext::function_traits<std::decay_t<Fn>>>;
		typename ext::function_arg_at_last<std::decay_t<Fn>>::type;
		requires std::is_lvalue_reference_v<typename ext::function_arg_at_last<std::decay_t<Fn>>::type>;
		requires std::derived_from<std::remove_cvref_t<typename ext::function_arg_at_last<std::decay_t<Fn>>::type>, Element>;
	};

	template <typename Fn>
	concept InvocableElemInitFunc = requires{
		requires ElemInitFunc<Fn>;
		requires std::invocable<Fn, std::add_lvalue_reference_t<std::remove_cvref_t<typename ext::function_arg_at_last<std::decay_t<Fn>>::type>>>;
	};

	template <typename InitFunc>
	struct ElemInitFuncTraits{
		using ElemType = std::remove_cvref_t<typename ext::function_arg_at_last<std::decay_t<InitFunc>>::type>;
		static_assert(std::derived_from<ElemType, Element>);
	};


	struct ElementUniquePtr{
		[[nodiscard]] constexpr ElementUniquePtr() = default;

		[[nodiscard]] constexpr explicit ElementUniquePtr(const ext::owner<Element*> element)
			: element{element}{}

		[[nodiscard]] ElementUniquePtr(Group* group, Scene* scene, const ext::owner<Element*> element)
			: element{element}{
			setGroupAndScene(group, scene);
		}

		template <InvocableElemInitFunc InitFunc>
		[[nodiscard]] explicit ElementUniquePtr(InitFunc initFunc)
			: element{new typename ElemInitFuncTraits<InitFunc>::ElemType{}}{
			std::invoke(initFunc,
			            static_cast<std::add_lvalue_reference_t<typename ElemInitFuncTraits<InitFunc>::ElemType>>(*element));
		}

		template <InvocableElemInitFunc InitFunc>
		[[nodiscard]] ElementUniquePtr(Group* group, Scene* scene, InitFunc initFunc)
			: element{new typename ElemInitFuncTraits<InitFunc>::ElemType{}}{
			setGroupAndScene(group, scene);
			std::invoke(initFunc,
			            static_cast<std::add_lvalue_reference_t<typename ElemInitFuncTraits<InitFunc>::ElemType>>(*element));
		}

		constexpr Element& operator*() const noexcept{
			assert(element != nullptr && "dereference on a null element");
			return *element;
		}

		constexpr Element* operator->() const noexcept{
			return element;
		}

		explicit constexpr operator bool() const noexcept{
			return element != nullptr;
		}

		[[nodiscard]] constexpr Element* get() const noexcept{
			return element;
		}

		[[nodiscard]] constexpr ext::owner<Element*> release() noexcept{
			return std::exchange(element, nullptr);
		}

		constexpr void reset() noexcept{
			this->operator=(ElementUniquePtr{});
		}

		constexpr void reset(const ext::owner<Element*> e) noexcept{
			this->operator=(ElementUniquePtr{e});
		}

		~ElementUniquePtr();

		constexpr friend bool operator==(const ElementUniquePtr& lhs, const ElementUniquePtr& rhs) noexcept = default;

		constexpr bool operator==(std::nullptr_t) const noexcept{
			return element != nullptr;
		}

		ElementUniquePtr(const ElementUniquePtr& other) = delete;

		constexpr ElementUniquePtr(ElementUniquePtr&& other) noexcept
			: element{other.release()}{
		}

		constexpr ElementUniquePtr& operator=(ElementUniquePtr&& other) noexcept{
			if(this == &other)return *this;
			std::swap(this->element, other.element);
			return *this;
		}

	private:
		ext::owner<Element*> element{};

		void setGroupAndScene(Group* group, Scene* scene) const;
	};
}

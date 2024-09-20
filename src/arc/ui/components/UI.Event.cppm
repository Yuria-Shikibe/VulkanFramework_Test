//
// Created by Matrix on 2024/9/12.
//

export module Core.UI.Event;

export import ext.event;
import Geom.Vector2D;
import Core.Ctrl.KeyPack;

export namespace Core::UI{
	struct PosedEvent : ext::event_type{
		Geom::Vec2 pos{};

		[[nodiscard]] PosedEvent() = default;

		[[nodiscard]] explicit PosedEvent(const Geom::Vec2& pos)
			: pos{pos}{}
	};

	struct MouseEvent : PosedEvent{
		Ctrl::KeyPack code{};

		[[nodiscard]] MouseEvent() = default;

		[[nodiscard]] MouseEvent(const Geom::Vec2 pos, const Ctrl::KeyPack code)
			: PosedEvent{pos},
			  code{code}{}

		constexpr decltype(auto) unpack() const noexcept{
			return code.unpack();
		}
	};

	namespace Event{
		struct Click final : MouseEvent{
			using MouseEvent::MouseEvent;
		};

		struct Drag final : MouseEvent{
			Geom::Vec2 dst{};

			[[nodiscard]] Drag() = default;

			[[nodiscard]] Drag(const Geom::Vec2 pos, const Geom::Vec2 dst, const Ctrl::KeyPack code)
				: MouseEvent{pos, code},
				  dst{dst}{}

			[[nodiscard]] constexpr Geom::Vec2 trans() const noexcept{
				return dst - pos;
			}
		};

		struct Inbound final : PosedEvent{using PosedEvent::PosedEvent;};
		struct Exbound final : PosedEvent{using PosedEvent::PosedEvent;};

		struct BeginFocus final : PosedEvent{using PosedEvent::PosedEvent;};
		struct EndFocus final : PosedEvent{using PosedEvent::PosedEvent;};

		struct Scroll final : PosedEvent{
			[[nodiscard]] Scroll() = default;

			[[nodiscard]] explicit Scroll(const Geom::Vec2& pos)
				: PosedEvent{pos}{}

			int mode{};
		};

	}

}

//
// Created by Matrix on 2024/9/12.
//

export module Core.Ctrl.KeyPack;

export import Core.Ctrl.Constants;
import std;

export namespace Core::Ctrl{
	constexpr unsigned KeyMask = 0xffff;

	using PackedKey = unsigned;

	/**
	* @code
	*	0b 0000'0000  0000'0000  0000'0000  0000'0000
	*	   [  Mode ]  [  Act  ]  [     Key Code     ]
	* @endcode
	*/
	constexpr PackedKey packKey(const int keyCode, const int act, const int mode) noexcept{
		return
			(keyCode & KeyMask)
			| (act & Act::Mask) << 16
			| (mode & Mode::Mask) << (16 + Act::Bits);
	}

	struct UnpackedKey{
		int key;
		int act;
		int mode;

		[[nodiscard]] constexpr PackedKey pack() const noexcept{
			return packKey(key, act, mode);
		}

		constexpr friend bool operator==(const UnpackedKey& lhs, const UnpackedKey& rhs) noexcept = default;
	};



	/**
	 * @brief
	 * @return [keyCode, act, mode]
	 */
	constexpr decltype(auto) unpackKey(const PackedKey fullKey) noexcept{
		return UnpackedKey{
				.key = static_cast<int>(fullKey & KeyMask),
				.act = static_cast<int>(fullKey >> 16 & Act::Mask),
				.mode = static_cast<int>(fullKey >> 24 & Mode::Mask)
			};
	}

	struct KeyPack{
		PackedKey code{};

		[[nodiscard]] constexpr KeyPack() noexcept = default;

		[[nodiscard]] constexpr explicit KeyPack(const PackedKey code)
			noexcept : code{code}{}

		[[nodiscard]] constexpr KeyPack(const int keyCode, const int act = Act::Ignore, const int mode = Mode::Ignore)
			noexcept : code{packKey(keyCode, act, mode)}{}

		[[nodiscard]] constexpr decltype(auto) unpack() const noexcept{
			return unpackKey(code);
		}

		constexpr friend bool operator==(const KeyPack& lhs, const KeyPack& rhs) noexcept = default;

		[[nodiscard]] constexpr int key() const noexcept{
			return static_cast<int>(code & KeyMask);
		}

		[[nodiscard]] constexpr int action() const noexcept{
			return static_cast<int>((code >> 16) & Act::Mask);
		}

		[[nodiscard]] constexpr int mode() const noexcept{
			return static_cast<int>((code >> 24) & Mode::Mask);
		}

		[[nodiscard]] constexpr bool matched(const KeyPack state) const noexcept{
			auto [tk, ta, tm] = unpack();
			auto [ok, oa, om] = state.unpack();
			return Key::matched(tk, ok) && Act::matched(ta, oa) && Mode::matched(tm, om);
		}
	};
}

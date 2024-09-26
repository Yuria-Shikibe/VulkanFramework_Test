//
// Created by Matrix on 2024/9/12.
//

export module Core.Ctrl.KeyPack;

import std;

export namespace Core::Ctrl{
	constexpr unsigned KeyMask = 0xffff;

	using PackedKey = unsigned;

	struct UnpackedKey{
		int keyCode;
		int act;
		int mode;
	};

	/**
	* @code
	*	0b 0000'0000  0000'0000  0000'0000  0000'0000
	*	   [  Mode ]  [  Act  ]  [     Key Code     ]
	* @endcode
	*/
	constexpr PackedKey packKey(const int keyCode, const int act, const int mode) noexcept{
		return
			(keyCode & KeyMask)
			| act << 16
			| mode << (16 + 8);
	}


	/**
	 * @brief
	 * @return [keyCode, act, mode]
	 */
	constexpr decltype(auto) unpackKey(const PackedKey fullKey) noexcept{
		return UnpackedKey{
				.keyCode = static_cast<int>(fullKey & KeyMask),
				.act = static_cast<int>(fullKey >> 16 & 0xff),
				.mode = static_cast<int>(fullKey >> 24 & 0xff)
			};
	}

	struct KeyPack{
		PackedKey code{};

		[[nodiscard]] constexpr KeyPack() = default;

		[[nodiscard]] constexpr explicit KeyPack(const PackedKey code)
			: code{code}{}

		[[nodiscard]] constexpr KeyPack(const int keyCode, const int act, const int mode)
			: code{packKey(keyCode, act, mode)}{}

		[[nodiscard]] constexpr decltype(auto) unpack() const noexcept{
			return unpackKey(code);
		}

		constexpr friend bool operator==(const KeyPack& lhs, const KeyPack& rhs) noexcept = default;

		[[nodiscard]] constexpr int key() const noexcept{
			return static_cast<int>(code & KeyMask);
		}

		[[nodiscard]] constexpr int action() const noexcept{
			return static_cast<int>((code >> 16) & 0xff);
		}

		[[nodiscard]] constexpr int mode() const noexcept{
			return static_cast<int>((code >> 24) & 0xff);
		}
	};


}

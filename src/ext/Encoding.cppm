module;

#if defined(_WIN32) || defined(_WIN64)
#define WIN_SYS
#include <Windows.h>
#else

#endif

export module ext.Encoding;

import std;

export namespace ext::encode{
	template <typename ToType>
	using MultiByteBuffer = std::array<ToType, 4 / sizeof(ToType)>;

	using CharBuffer = std::array<char, 4>;

	char32_t convertTo(const char* charCodes, const int size = 4){
		const unsigned char c = charCodes[0];
		switch(size){
			case 2 : return (c & 0x1F) <<  6 | (charCodes[1] & 0x3F);
			case 3 : return (c & 0x0F) << 12 | (charCodes[1] & 0x3F) <<  6 | (charCodes[2] & 0x3F);
			case 4 : return (c & 0x07) << 18 | (charCodes[1] & 0x3F) << 12 | (charCodes[2] & 0x3F) << 6 | charCodes[3] & 0x3F;
			default: return c;
		}
	}

	template <typename ToType, typename FromType>
	MultiByteBuffer<ToType> convertTo(FromType charCode){
		MultiByteBuffer<ToType> buffer{};
#ifdef WIN_SYS
		int failed{};

		WideCharToMultiByte(
			CP_UTF8, 0,
			reinterpret_cast<wchar_t*>(&charCode), 1,
			reinterpret_cast<char*>(buffer.data()), buffer.size() * sizeof(ToType),
			nullptr, &failed);

		if(failed){
			//TODO warning or exception here
		}
#endif
		return buffer;
	}

	template <typename ToType, typename FromType>
	void convertTo(FromType charCode, MultiByteBuffer<ToType> buffer){
#ifdef WIN_SYS
		int failed{};

		WideCharToMultiByte(
			CP_UTF8, 0,
			reinterpret_cast<wchar_t*>(&charCode), 1,
			reinterpret_cast<char*>(buffer.data()), buffer.size() * sizeof(ToType),
			nullptr, &failed);

		if(failed){
			//TODO warning or exception here
		}
#endif
	}

	template <typename FromType, typename ToType>
	std::basic_string<ToType> convertTo(const std::basic_string_view<FromType> src){
		std::basic_string<ToType> str{};

		str.reserve(str.size() * (sizeof(FromType) / sizeof(ToType)));

		for(const auto value : src){
			str.append_range(convertTo<char32_t, char>(value));
		}

		return str;
	}

	/**
	 * @tparam To Dest Char Type
	 * @param from Src String [ASCII Only]
	 */
	template <typename To>
	std::basic_string<To> convertTo(const std::string_view from){
		std::basic_string<To> to{};
		to.reserve(from.size());
		std::ranges::copy(from, std::back_inserter(to));
		return to;
	}

	/**
	 * @tparam To Dest Char Type
	 * @param from Src String [ASCII Only]
	 */
	std::wstring convertToWide(const std::string_view from){
		std::wstring to{};
		to.reserve(from.size());
		std::ranges::copy(from, std::back_inserter(to));
		return to;
	}

	constexpr bool isUnicodeHead(const char code){
		constexpr char Mask = 0b0000'0001;

		// 0b'10...
		if((code >> 7 & Mask) == 1 && (code >> 6 & Mask) == 0){
			return false;
		}

		return true;
	}

	/**
	 * @brief Warning: this function assume that inItr is always derreferenceable
	 * @param inItr Search Pos
	 */
	template <std::contiguous_iterator Itr>
		requires std::same_as<typename std::iterator_traits<Itr>::value_type, char>
	[[nodiscard]] Itr gotoUnicodeHead(Itr inItr){
		while(!ext::encode::isUnicodeHead(inItr.operator*())){
			--inItr;
		}

		return inItr;
	}

	/**
	 * @brief
	 * @param code Unicode Head
	 * @return 0 if this code is not a unicode head
	 */
	template <std::integral Ret = unsigned>
	constexpr Ret getUnicodeLength(const char code){
		//TODO check if this code is really a unicode character
		static constexpr char Mask = 0b0000'0001;

		if(code >> 7 & Mask){ //0b'1...
			if(code >> 6 & Mask){ //0b'11...
				if(code >> 5 & Mask){ //0b'111...
					if(code >> 4 & Mask){ //0b'1111...
						if(code >> 3 & Mask){ //0b'11111... ,
							if(code >> 2 & Mask){ //0b'111111... ,
								if(code >> 1 & Mask){ //0b'1111111... ,
									if(code >> 0 & Mask){
										return 8;
									}

									return 7;
								}

								return 6; //0b'11111... ,
							}

							return 5; //0b'11111... ,
						}
						//0b'11110... ,4
						return 4;
					}
					//0b'1110... ,3
					return 3;
				}
				//0b'110... ,2
				return 2;
			}
			//0b'10... ,Not Head
			return 0;
		}

		//0b'0... ,ASCII
		return 1;
	}

	template <std::ranges::contiguous_range Rng>
		requires (std::ranges::sized_range<Rng> && sizeof(std::ranges::range_value_t<Rng>) == 1)
	auto convertCharToChar32(Rng&& src){
		std::vector<char32_t> dest{};
		const std::size_t totalSize = std::ranges::size(src);

		dest.reserve(totalSize * 3);
		for(std::size_t index = 0; index < totalSize; ++index){
			const auto cur = src[index];
			if(cur >= 0 && cur <= std::invoke(std::numeric_limits<char>::max) && std::isspace(cur))continue;

			unsigned int charCode = static_cast<unsigned char>(cur);
			const int charCodeLength = ext::encode::getUnicodeLength<int>(cur);

			if(charCodeLength > 1 && index + charCodeLength <= totalSize){
				charCode = ext::encode::convertTo(std::ranges::data(src) + index, charCodeLength);
			}

			dest.push_back(charCode);

			index += charCodeLength - 1;
		}

		return dest;
	}
}

export std::ostream& operator<<(std::ostream& stream, const ext::encode::CharBuffer buffer){
	// stream.write(buffer.data(), 2);

	stream << std::string{buffer.data(), ext::encode::getUnicodeLength(buffer.front())};
	return stream;
}

export
template <>
struct std::formatter<ext::encode::CharBuffer> {
	static constexpr auto parse(auto& ctx){return ctx.begin();}

	template<class FmtContext>
	static typename FmtContext::iterator format(const ext::encode::CharBuffer buffer, FmtContext& ctx){
		std::ostringstream out{};
		out << std::string_view{buffer.data(), ext::encode::getUnicodeLength(buffer.front())};

		return std::ranges::copy(std::move(out).str(), ctx.out()).out;
	}
};


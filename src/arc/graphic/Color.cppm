export module Graphic.Color;

import std;
import Math;
import Geom.Vector3D;

export namespace Graphic{

	/**
	 * \brief  32Bits for 4 u byte[0, 255]
	 * \code
	 * 00000000__00000000__00000000__00000000
	 * r value^  g value^  b value^  a value^
	 *       24        16         8         0
	 * \endcode
	 *
	 * TODO HDR support
	 */
	class Color{
	public:
	    static constexpr float LightColorRange = 2550.f;

		static constexpr auto MaxVal           = std::numeric_limits<std::uint8_t>::max();
		static constexpr float maxValF         = std::numeric_limits<std::uint8_t>::max();
		static constexpr unsigned int r_Offset = 24;
		static constexpr unsigned int g_Offset = 16;
		static constexpr unsigned int b_Offset = 8 ;
		static constexpr unsigned int a_Offset = 0 ;
		static constexpr unsigned int a_Mask   = 0x00'00'00'ff;
		static constexpr unsigned int b_Mask   = 0x00'00'ff'00;
		static constexpr unsigned int g_Mask   = 0x00'ff'00'00;
		static constexpr unsigned int r_Mask   = 0xff'00'00'00;

		float r{1.0f};
		float g{1.0f};
		float b{1.0f};
		float a{1.0f};

		using ColorBits = unsigned int;

		constexpr Color() = default;

		constexpr explicit Color(const ColorBits rgba8888V){
			rgba8888(rgba8888V);
		}

		constexpr Color(const float r, const float g, const float b, const float a)
			: r(r),
			  g(g),
			  b(b),
			  a(a){
		}

		constexpr Color(const float r, const float g, const float b): Color(r, g, b, 1){}
	private:
		template <bool doClamp>
		constexpr Color& clampCond() noexcept{
			if constexpr (doClamp){
				return clamp();
			}else{
				return *this;
			}
		}

	public:
		//TODO is this really good??
	    constexpr Color& appendLightColor(const Color& color) {
	        r += static_cast<float>(Math::floor(LightColorRange * color.r, 10));
	        g += static_cast<float>(Math::floor(LightColorRange * color.g, 10));
	        b += static_cast<float>(Math::floor(LightColorRange * color.b, 10));
	        a += static_cast<float>(Math::floor(LightColorRange * color.a, 10));

	        return *this;
	    }

	    constexpr Color& toLightColor() {
	        r = static_cast<float>(Math::floor(LightColorRange * r, 10));
	        g = static_cast<float>(Math::floor(LightColorRange * g, 10));
	        b = static_cast<float>(Math::floor(LightColorRange * b, 10));
	        a = static_cast<float>(Math::floor(LightColorRange * a, 10));

	        return *this;
	    }

		friend constexpr std::size_t hash_value(const Color& obj){
			return obj.hashCode();
		}

		[[nodiscard]] std::string string() const {
		    return  std::format("[{:02}, {:02}, {:02}, {:02}]", r * MaxVal, g * MaxVal, b * MaxVal, a * MaxVal);
	    }

		friend std::ostream& operator<<(std::ostream& os, const Color& obj){
			return os << obj.string();
		}

		constexpr friend bool operator==(const Color& lhs, const Color& rhs) noexcept = default;

	    static auto stringToRgba(const std::string_view hexStr){
		    std::array<std::uint8_t, 4> rgba{};
		    for(const auto& [index, v1] : hexStr
		        | std::views::slide(2)
		        | std::views::stride(2)
		        | std::views::take(4)
		        | std::views::enumerate){
			    std::from_chars(v1.data(), v1.data() + v1.size(), rgba[index], 16);
		    }

		    return rgba;
	    }

	    static ColorBits stringToRgbaBits(const std::string_view hexStr){
	    	std::uint32_t value = std::bit_cast<std::uint32_t>(stringToRgba(hexStr));
	    	if constexpr (std::endian::native == std::endian::little) {
	    		value = std::byteswap(value);
	    	}

	    	return value;
	    }

	    static Color valueOf(const std::string_view hexStr){
	    	const auto rgba = stringToRgba(hexStr);

	    	Color color{};

	    	color.r = static_cast<float>(rgba[0]) / static_cast<float>(std::numeric_limits<std::uint8_t>::max());
	    	color.g = static_cast<float>(rgba[1]) / static_cast<float>(std::numeric_limits<std::uint8_t>::max());
	    	color.b = static_cast<float>(rgba[2]) / static_cast<float>(std::numeric_limits<std::uint8_t>::max());
	    	color.a = static_cast<float>(rgba[3]) / static_cast<float>(std::numeric_limits<std::uint8_t>::max());

	    	return color;
	    }
	
		static constexpr ColorBits rgb888(const float r, const float g, const float b) noexcept{
			return static_cast<ColorBits>(r * MaxVal) << 16 | static_cast<ColorBits>(g * MaxVal) << 8 | static_cast<ColorBits>(b * MaxVal);
		}

	    static constexpr ColorBits rgba8888(const float r, const float g, const float b, const float a) noexcept{
		    return
			    static_cast<ColorBits>(r * MaxVal) << r_Offset & r_Mask |
			    static_cast<ColorBits>(g * MaxVal) << g_Offset & g_Mask |
			    static_cast<ColorBits>(b * MaxVal) << b_Offset & b_Mask |
			    static_cast<ColorBits>(a * MaxVal) << a_Offset & a_Mask;
	    }
		
		[[nodiscard]] constexpr ColorBits rgb888() const noexcept{
			return rgb888(r, g, b);
		}

		[[nodiscard]] constexpr ColorBits rgba8888() const noexcept{
			return rgba8888(r, g, b, a);
		}

		[[nodiscard]] constexpr ColorBits rgba() const noexcept{
	    	return rgba8888();
	    }

		constexpr Color& rgb888(const ColorBits value) noexcept{
			r = static_cast<float>(value >> 16 & MaxVal) / maxValF;
			g = static_cast<float>(value >> 8  & MaxVal) / maxValF;
			b = static_cast<float>(value >> 0  & MaxVal) / maxValF;
			return *this;
		}

		constexpr Color& rgba8888(const ColorBits value) noexcept{
			r = static_cast<float>(value >> r_Offset & MaxVal) / maxValF;
			g = static_cast<float>(value >> g_Offset & MaxVal) / maxValF;
			b = static_cast<float>(value >> b_Offset & MaxVal) / maxValF;
			a = static_cast<float>(value >> a_Offset & MaxVal) / maxValF;
			return *this;
		}

		[[nodiscard]] float diff(const Color& other) const noexcept{
			return Math::abs(hue() - other.hue()) / 360 + Math::abs(value() - other.value()) + Math::abs(saturation() - other.saturation());
		}

		constexpr Color& set(const Color& color) noexcept{
			return this->operator=(color);
		}

		//TODO operator overload?

		template <bool doClamp = true>
		constexpr Color& mul(const Color& color) noexcept{
			this->r *= color.r;
			this->g *= color.g;
			this->b *= color.b;
			this->a *= color.a;
			return clampCond<doClamp>();
		}

		template <bool doClamp = true>
		constexpr Color& mul_rgb(const float value) noexcept{
			this->r *= value;
			this->g *= value;
			this->b *= value;
			return clampCond<doClamp>();
		}

		template <bool doClamp = true>
		constexpr Color& mul_rgba(const float value) noexcept{
			this->r *= value;
			this->g *= value;
			this->b *= value;
			this->a *= value;
			return clampCond<doClamp>();
		}

		template <bool doClamp = true>
		constexpr Color& add(const Color& color) noexcept{
			this->r += color.r;
			this->g += color.g;
			this->b += color.b;
			return clampCond<doClamp>();
		}

		template <bool doClamp = true>
		constexpr Color& sub(const Color& color) noexcept{
			this->r -= color.r;
			this->g -= color.g;
			this->b -= color.b;
			return clampCond<doClamp>();
		}

		constexpr Color& clamp() noexcept{
	    	r = Math::clamp(r);
	    	g = Math::clamp(g);
	    	b = Math::clamp(b);
	    	a = Math::clamp(a);

			return *this;
		}


		template <bool doClamp = true>
		constexpr Color& set(const float tr, const float tg, const float tb, const float ta) noexcept{
			this->r = tr;
			this->g = tg;
			this->b = tb;
			this->a = ta;
			return clampCond<doClamp>();
		}


		constexpr Color& setForce(const float tr, const float tg, const float tb, const float ta) noexcept{
	    	return set<false>(tr, tg, tb, ta);
	    }

		template <bool doClamp = true>
		constexpr Color& set(const float tr, const float tg, const float tb) noexcept{
			this->r = tr;
			this->g = tg;
			this->b = tb;
			return clampCond<doClamp>();
		}

		constexpr Color& set(const ColorBits rgba) noexcept{
			return rgba8888(rgba);
		}

		[[nodiscard]] constexpr float sum() const noexcept{
			return r + g + b;
		}

		template <bool doClamp = true>
		constexpr Color& add(const float tr, const float tg, const float tb, const float ta) noexcept{
			this->r += tr;
			this->g += tg;
			this->b += tb;
			this->a += ta;
			return clampCond<doClamp>();
		}

		template <bool doClamp = true>
		constexpr Color& add(const float tr, const float tg, const float tb) noexcept{
			this->r += tr;
			this->g += tg;
			this->b += tb;
			return clampCond<doClamp>();
		}

		template <bool doClamp = true>
		constexpr Color& sub(const float tr, const float tg, const float tb, const float ta) noexcept{
			this->r -= tr;
			this->g -= tg;
			this->b -= tb;
			this->a -= ta;
			return clampCond<doClamp>();
		}

		template <bool doClamp = true>
		constexpr Color& sub(const float tr, const float tg, const float tb) noexcept{
			this->r -= tr;
			this->g -= tg;
			this->b -= tb;
			return clampCond<doClamp>();
		}

		constexpr Color& invRgb(){
			r = 1 - r;
			g = 1 - g;
			b = 1 - b;
			return *this;
		}

		constexpr Color& setR(const float tr) noexcept{
			this->r -= tr;
			return *this;
		}

		constexpr Color& setG(const float tg) noexcept{
			this->g -= tg;
			return *this;
		}

		constexpr Color& setB(const float tb) noexcept{
			this->b -= tb;
			return *this;
		}

		constexpr Color& setA(const float ta) noexcept{
			this->a = ta;
			return *this;
		}

		constexpr Color& mulA(const float ta) noexcept{
			this->a *= ta;
			return *this;
		}

		template <bool doClamp = true>
		constexpr Color& mul(const float tr, const float tg, const float tb, const float ta) noexcept{
			this->r *= tr;
			this->g *= tg;
			this->b *= tb;
			this->a *= ta;
			return clampCond<doClamp>();
		}

		template <bool doClamp = true>
		constexpr Color& mul(const float val) noexcept{
			this->r *= val;
			this->g *= val;
			this->b *= val;
			this->a *= val;
			return clampCond<doClamp>();
		}

		template <bool doClamp = true>
		constexpr Color& lerp(const Color& target, const float t) noexcept{
			this->r += t * (target.r - this->r);
			this->g += t * (target.g - this->g);
			this->b += t * (target.b - this->b);
			this->a += t * (target.a - this->a);
			return clampCond<doClamp>();
		}

		template <bool doClamp = true>
		constexpr Color& lerpRGB(const Color& target, const float t) noexcept{
			this->r += t * (target.r - this->r);
			this->g += t * (target.g - this->g);
			this->b += t * (target.b - this->b);
			return clampCond<doClamp>();
		}

		template <bool doClamp = true>
		[[nodiscard]] constexpr Color createLerp(const Color& target, const float t) const noexcept{
			Color newColor{
				r + t * (target.r - r),
				g + t * (target.g - g),
				b + t * (target.b - b),
				a + t * (target.a - a)
			};

			return newColor.clampCond<doClamp>();
		}

		template <bool doClamp = true>
		constexpr Color& lerp(const float tr, const float tg, const float tb, const float ta, const float t) noexcept{
			this->r += t * (tr - this->r);
			this->g += t * (tg - this->g);
			this->b += t * (tb - this->b);
			this->a += t * (ta - this->a);
			return clampCond<doClamp>();
		}

		constexpr Color& mulSelfAlpha() noexcept{
			r *= a;
			g *= a;
			b *= a;
			return *this;
		}

		constexpr Color& write(Color& to) const noexcept{
			return to.set(*this);
		}

		using HSVType = std::array<float, 3>;

		[[nodiscard]] constexpr float hue() const noexcept{
			return toHsv()[0];
		}

		[[nodiscard]] constexpr float saturation() const noexcept{
			return toHsv()[1];
		}

		[[nodiscard]] constexpr float value() const noexcept{
			return toHsv()[2];
		}

		Color& byHue(const float amount) noexcept{
			HSVType TMP_HSV = toHsv();
			TMP_HSV[0] = amount;
			fromHsv(TMP_HSV);
			return *this;
		}

		Color& bySaturation(const float amount) noexcept{
			HSVType TMP_HSV = toHsv();
			TMP_HSV[1] = amount;
			fromHsv(TMP_HSV);
			return *this;
		}

		Color& byValue(const float amount) noexcept{
			HSVType TMP_HSV = toHsv();
			TMP_HSV[2] = amount;
			fromHsv(TMP_HSV);
			return *this;
		}

		Color& shiftHue(const float amount) noexcept{
			HSVType TMP_HSV = toHsv();
			TMP_HSV[0] += amount;
			fromHsv(TMP_HSV);
			return *this;
		}

		Color& shiftSaturation(const float amount) noexcept{
			HSVType TMP_HSV = toHsv();
			TMP_HSV[1] += amount;
			fromHsv(TMP_HSV);
			return *this;
		}

		Color& shiftValue(const float amount) noexcept{
			HSVType TMP_HSV = toHsv();
			TMP_HSV[2] += amount;
			fromHsv(TMP_HSV);
			return *this;
		}

		[[nodiscard]] constexpr std::size_t hashCode() const noexcept{
			return rgba8888();
		}

		[[nodiscard]] constexpr ColorBits abgr() const noexcept{
			return static_cast<ColorBits>(255 * a) << 24 | static_cast<ColorBits>(255 * b) << 16 | static_cast<ColorBits>(255 * g) << 8 | static_cast<ColorBits>(255 * r);
		}

		[[nodiscard]] std::string toString() const{
			return std::format("{:02X}{:02X}{:02X}{:02X}", static_cast<ColorBits>(255 * r), static_cast<ColorBits>(255 * g), static_cast<ColorBits>(255 * b), static_cast<ColorBits>(255 * a));;
		}

		Color& fromHsv(const float h, const float s, const float v) noexcept{
			const float x = std::fmod(h / 60.0f + 6, static_cast<float>(6));
			const int i = static_cast<int>(x);
			const float f = x - static_cast<float>(i);
			const float p = v * (1 - s);
			const float q = v * (1 - s * f);
			const float t = v * (1 - s * (1 - f));
			switch (i) {
				case 0:
					r = v;
					g = t;
					b = p;
					break;
				case 1:
					r = q;
					g = v;
					b = p;
					break;
				case 2:
					r = p;
					g = v;
					b = t;
					break;
				case 3:
					r = p;
					g = q;
					b = v;
					break;
				case 4:
					r = t;
					g = p;
					b = v;
					break;
				default:
					r = v;
					g = p;
					b = q;
			}

			return clamp();
		}

		Color& fromHsv(const HSVType hsv) noexcept{
			return fromHsv(hsv[0], hsv[1], hsv[2]);
		}

		constexpr Color HSVtoRGB(const float h, const float s, const float v, const float alpha) noexcept{
			Color c = HSVtoRGB(h, s, v);
			c.a = alpha;
			return c;
		}

		constexpr Color HSVtoRGB(const float h, const float s, const float v) noexcept{
			Color c{ 1, 1, 1, 1 };
			HSVtoRGB(h, s, v, c);
			return c;
		}

		[[nodiscard]] constexpr HSVType toHsv() const noexcept{
			HSVType hsv = {};

			const float maxV = Math::max(Math::max(r, g), b);
			const float minV = Math::min(Math::min(r, g), b);
			if (const float range = maxV - minV; range == 0) {
				hsv[0] = 0;
			}
			else if (maxV == r) {
				hsv[0] = std::fmod(60 * (g - b) / range + 360, 360.0f);
			}
			else if (maxV == g) {
				hsv[0] = 60 * (b - r) / range + 120;
			}
			else {
				hsv[0] = 60 * (r - g) / range + 240;
			}

			if (maxV > 0) {
				hsv[1] = 1 - minV / maxV;
			}
			else {
				hsv[1] = 0;
			}

			hsv[2] = maxV;

			return hsv;
		}

		[[nodiscard]] bool equalRelaxed(const Color& other) const noexcept{
			constexpr float tolerance = 0.5f / maxValF;
			return
				Math::equal(r, other.r, tolerance) &&
				Math::equal(g, other.g, tolerance) &&
				Math::equal(b, other.b, tolerance) &&
				Math::equal(a, other.a, tolerance);
		}

		constexpr Color& HSVtoRGB(float h, float s, float v, Color& targetColor) noexcept{
			if (h == 360) h = 359;
			h = Math::max(0.0f, Math::min(360.0f, h));
			s = Math::max(0.0f, Math::min(100.0f, s));
			v = Math::max(0.0f, Math::min(100.0f, v));
			s /= 100.0f;
			v /= 100.0f;
			h /= 60.0f;
			const int i = Math::floorLEqual(h);
			const float f = h - static_cast<float>(i);
			const float p = v * (1 - s);
			const float q = v * (1 - s * f);
			const float t = v * (1 - s * (1 - f));
			switch (i) {
				case 0:
					r = v;
					g = t;
					b = p;
					break;
				case 1:
					r = q;
					g = v;
					b = p;
					break;
				case 2:
					r = p;
					g = v;
					b = t;
					break;
				case 3:
					r = p;
					g = q;
					b = v;
					break;
				case 4:
					r = t;
					g = p;
					b = v;
					break;
				default:
					r = v;
					g = p;
					b = q;
			}

			targetColor.setA(targetColor.a);
			return targetColor;
		}

		template <std::ranges::random_access_range Rng>
			requires (std::ranges::sized_range<Rng> && std::convertible_to<const Color&, std::ranges::range_value_t<Rng>>)
		[[nodiscard]] static constexpr Color getLerpVal(float s, const Rng& colors) noexcept{
			s = Math::clamp(s);
			const std::size_t size = std::ranges::size(colors);
			const std::size_t bound = size - 1;

			const auto boundf = static_cast<float>(size);

			const Color& ca = std::ranges::begin(colors)[static_cast<std::size_t>(s * boundf)];
			const Color& cb = std::ranges::begin(colors)[Math::min(static_cast<std::size_t>(s * boundf + 1), bound)];

			const float n = s * boundf - static_cast<float>(static_cast<int>(s * boundf));
			const float i = 1.0f - n;
			return Color{ ca.r * i + cb.r * n, ca.g * i + cb.g * n, ca.b * i + cb.b * n, ca.a * i + cb.a * n};
		}


		[[nodiscard]] static constexpr Color getLerpVal(float s, const auto&... colors) noexcept{
			return Color::getLerpVal(s, std::array{colors...});
		}

		static constexpr Color createLerp(const float s, const auto&... colors) noexcept{
			return Color::getLerpVal(s, colors...);
		}

		constexpr Color& lerp(const float s, const auto&... colors) noexcept{
			return this->set(Color::getLerpVal(s, colors...));
		}

		template <std::ranges::random_access_range Rng>
			requires (std::ranges::sized_range<Rng> && std::convertible_to<const Color&, std::ranges::range_value_t<Rng>>)
		constexpr Color& lerp(const float s, const Rng& colors) noexcept{
			return this->operator=(Color::getLerpVal(s, colors));
		}

		[[nodiscard]] constexpr Color copy() const noexcept{
			return {*this};
		}
	};

	namespace Colors {
		constexpr Color WHITE{ 1, 1, 1, 1 };
		constexpr Color LIGHT_GRAY{ 0xbfbfbfff };
		constexpr Color GRAY{ 0x7f7f7fff };
		constexpr Color DARK_GRAY{ 0x3f3f3fff };
		constexpr Color BLACK{ 0, 0, 0, 1 };
		constexpr Color CLEAR{ 0, 0, 0, 0 };

		constexpr Color BLUE{ 0, 0, 1, 1 };
		constexpr Color NAVY{ 0, 0, 0.5f, 1 };
		constexpr Color ROYAL{ 0x4169e1ff };
		constexpr Color SLATE{ 0x708090ff };
		constexpr Color SKY{ 0x87ceebff };

		constexpr Color AQUA{ 0x85A2F3ff };
		constexpr Color BLUE_SKY = Color::createLerp(0.745f, BLUE, SKY);
		constexpr Color AQUA_SKY = Color::createLerp(0.5f, AQUA, SKY);

		constexpr Color CYAN{ 0, 1, 1, 1 };
		constexpr Color TEAL{ 0, 0.5f, 0.5f, 1 };

		constexpr Color GREEN{ 0x00ff00ff };
		constexpr Color PALE_GREEN{ 0xa1ecabff };
		constexpr Color ACID{ 0x7fff00ff };
		constexpr Color LIME{ 0x32cd32ff };
		constexpr Color FOREST{ 0x228b22ff };
		constexpr Color OLIVE{ 0x6b8e23ff };

		constexpr Color YELLOW{ 0xffff00ff };
		constexpr Color GOLD{ 0xffd700ff };
		constexpr Color GOLDENROD{ 0xdaa520ff };
		constexpr Color ORANGE{ 0xffa500ff };

		constexpr Color BROWN{ 0x8b4513ff };
		constexpr Color TAN{ 0xd2b48cff };
		constexpr Color BRICK{ 0xb22222ff };

		constexpr Color RED{ 0xff0000ff };
		constexpr Color RED_DUSK{ 0xDE6663ff };
		constexpr Color SCARLET{ 0xff341cff };
		constexpr Color CRIMSON{ 0xdc143cff };
		constexpr Color CORAL{ 0xff7f50ff };
		constexpr Color SALMON{ 0xfa8072ff };
		constexpr Color PINK{ 0xff69b4ff };
		constexpr Color MAGENTA{ 1, 0, 1, 1 };

		constexpr Color PURPLE{ 0xa020f0ff };
		constexpr Color VIOLET{ 0xee82eeff };
		constexpr Color MAROON{ 0xb03060ff };
	}
}

export
	template<>
	struct ::std::hash<Graphic::Color>{
		size_t operator()(const Graphic::Color& obj) const noexcept{
			return obj.hashCode();
		}
	};

// export namespace Colors = Graphic::Colors;

//
// Created by Matrix on 2024/3/8.
//

export module Math.Timed;

export import Math;
export import Math.Interpolation;
import ext.concepts;
import std;

//TODO is this namespace appropriate?
export namespace Math{
	struct Timed {
		float lifetime{};
		float time{};



		void setLifetime(const float l){
			this->lifetime = l;
		}

		template <bool autoClamp = false>
		constexpr void set(const float time, const float lifetime){
			if constexpr (autoClamp){
				this->lifetime = max(lifetime, 0.0f);
				this->time = clamp(time, 0.0f, lifetime);
			}else{
				this->lifetime = lifetime;
				this->time = time;
			}

		}

		explicit constexpr operator bool() const noexcept{
			return time >= lifetime;
		}


		/**
		 * @param delta advance time delta
		 * @return if time reached lifetime
		 */
		bool advance(const float delta) noexcept{
			time += delta;
			if(time >= lifetime){
				time = lifetime;
				return true;
			}

			return false;
		}

		template <bool autoClamp = false>
		constexpr void update(const float delta) noexcept{
			time += delta;

			if constexpr (autoClamp){
				time = clamp(time, 0.0f, lifetime);
			}
		}

		template <bool autoClamp = false>
		[[nodiscard]] constexpr float get() const noexcept{
			if constexpr (autoClamp){
				return clamp(time / lifetime);
			}

			return time / lifetime;
		}

		template <bool autoClamp = true>
		[[nodiscard]] constexpr float getWith(const float otherLifetime) const{
			if constexpr (autoClamp){
				return clamp(time / otherLifetime);
			}

			return time / otherLifetime;
		}

		template <ext::invocable<float(float)> Func>
		[[nodiscard]] float get(Func interp) const{
			return std::invoke(interp, get());
		}

		/**
		 * @brief [margin, 1]
		 */
		[[nodiscard]] constexpr float getMargin(const float margin) const noexcept{
			return margin + get() * (1 - margin);
		}

		[[nodiscard]] constexpr float getInv() const noexcept{
			return 1 - get();
		}

		[[nodiscard]] float getSlope() const noexcept{
			return slope(get());
		}
	};
}

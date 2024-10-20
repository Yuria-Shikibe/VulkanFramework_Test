module;

#include <cassert>

export module Math.Rand;

import std;

export namespace Math {
	/**
	 * TODO adapt to std::random_generator
	 * Impl XorShift Random
	 * @author Inferno
	 * @author davebaol
	 */
	class Rand {
	public:
		using SeedType = std::size_t;

	private:
		/** Normalization constant for double. */
		static constexpr double NORM_DOUBLE = 1.0 / static_cast<double>(1ull << 53);
		/** Normalization constant for float. */
		static constexpr float NORM_FLOAT = 1.0f / static_cast<float>(1ull << 24);

		static constexpr SeedType murmurHash3(SeedType x) noexcept {
			x ^= x >> 33;
			x *= 0xff51afd7ed558ccdL;
			x ^= x >> 33;
			x *= 0xc4ceb9fe1a85ec53L;
			x ^= x >> 33;

			return x;
		}

		/** The first half of the internal state of this pseudo-random number generator. */
		SeedType seed0{};
		/** The second half of the internal state of this pseudo-random number generator. */
		SeedType seed1{};

		// constexpr int next(const int bits) noexcept {
		// 	return static_cast<int>(next() & (1ull << bits) - 1ull);
		// }

	public:
		Rand() noexcept { // NOLINT(*-use-equals-default)
			setSeed(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
		}

		/**
		 * @param seed the initial seed
		 */
		constexpr explicit Rand(const SeedType seed) noexcept {
			setSeed(seed);
		}

		/**
		 * @param seed0 the first part of the initial seed
		 * @param seed1 the second part of the initial seed
		 */
		constexpr Rand(const SeedType seed0, const SeedType seed1) noexcept {
			setState(seed0, seed1);
		}
		
		constexpr std::size_t next() noexcept {
			SeedType s1       = this->seed0;
			const SeedType s0 = this->seed1;
			this->seed0       = s0;
			s1 ^= s1 << 23;
			return (this->seed1 = s1 ^ s0 ^ s1 >> 17ull ^ s0 >> 26ull) + s0;
		}

		template <typename T>
			requires (std::is_arithmetic_v<T>)
		constexpr T getNext() noexcept{
			if constexpr (std::integral<T>){
				return static_cast<T>(next());
			}else if constexpr (std::floating_point<T>){
				if constexpr (std::same_as<T, float>){
					return static_cast<float>(next() >> 40) * NORM_FLOAT; // NOLINT(*-narrowing-conversions)
				}else if constexpr (std::same_as<T, double>){
					return static_cast<double>(next() >> 11) * NORM_DOUBLE;
				}else{
					static_assert(false, "unsupported floating type");
				}
			}else if constexpr (std::same_as<T, bool>){
				return (next() & 1) != 0;
			}else{
				static_assert(false, "unsupported arithmetic type");
			}
		}

		/**
		 * Returns the next pseudo-random, uniformly distributed {int} value from this random number generator's sequence.
		 */
		constexpr int nextInt() noexcept {
			return static_cast<int>(next());
		}

		/**
		 * @param n_exclusive the positive bound on the random number to be returned.
		 * @return the next pseudo-random {int} value between {0} (inclusive) and {n} (exclusive).
		 */
		constexpr int nextInt(const int n_exclusive) noexcept {
			return next(n_exclusive);
		}

		/**
		 * Returns a pseudo-random, uniformly distributed {long} value between 0 (inclusive) and the specified value (exclusive),
		 * drawn from this random number generator's sequence. The algorithm used to generate the value guarantees that the result is
		 * uniform, provided that the sequence of 64-bit values produced by this generator is.
		 * @param n_exclusive the positive bound on the random number to be returned.
		 * @return (0, n]
		 */
		constexpr std::size_t next(const std::size_t n_exclusive) noexcept {
			assert(n_exclusive != 0);
			for(;;) {
				const std::size_t bits  = next() >> 1ull;
				const std::size_t value = bits % n_exclusive;
				if(bits >= value + (n_exclusive - 1)) return value;
			}
		}

		/**
		 * The given seed is passed twice through a hash function. This way, if the user passes a small value we avoid the short
		 * irregular transient associated with states having a very small number of bits set.
		 * @param _seed a nonzero seed for this generator (if zero, the generator will be seeded with @link std::numeric_limits<SeedType>::lowest() @endlink ).
		 */
		constexpr void setSeed(const SeedType _seed) noexcept {
			const SeedType seed0 = murmurHash3(_seed == 0 ? std::numeric_limits<SeedType>::lowest() : _seed);
			setState(seed0, murmurHash3(seed0));
		}

		template <std::floating_point T>
		constexpr bool chance(const T chance) noexcept {
			return getNext<T>() < chance;
		}

		template <typename T>
			requires (std::is_arithmetic_v<T>)
		constexpr T range(const T amount) noexcept {
			if constexpr (std::integral<T>){
				//TODO support unsigned to signed??
				return this->nextInt(amount * 2 + 1) - amount;
			}else if constexpr (std::floating_point<T>){
				return getNext<T>() * amount * 2 - amount;
			}else{
				static_assert(false, "unsupported arithmetic type");
			}

		}

		template <typename T>
			requires (std::is_arithmetic_v<T>)
		constexpr T random(const T max_inclusive) noexcept {
			if constexpr (std::integral<T>){
				//TODO support unsigned to signed??
				return this->nextInt(max_inclusive + 1);
			}else if constexpr (std::floating_point<T>){
				return getNext<T>() * max_inclusive;;
			}else{
				static_assert(false, "unsupported arithmetic type");
			}
		}

		template <typename T1, typename T2>
			requires (std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>)
		constexpr auto random(const T1 min, const T2 max) noexcept -> std::common_type_t<T1, T2> {
			using T = std::common_type_t<T1, T2>;

			if constexpr (std::integral<T>){
				assert(min <= max);
				return min + this->nextInt(max - min + 1);
			}else{
				return min + (max - min) * getNext<T>();
			}
		}

		template <typename T = float>
			requires (std::is_arithmetic_v<T>)
		constexpr T randomDirection() noexcept{
			return this->random<T>(static_cast<T>(360));
		}

		/**
		 * Sets the internal state of this generator.
		 * @param s0 the first part of the internal state
		 * @param s1 the second part of the internal state
		 */
		constexpr void setState(const SeedType s0, const SeedType s1) noexcept {
			this->seed0 = s0;
			this->seed1 = s1;
		}

		/**
		 * OPTM WTF??
		 * Returns the internal seeds to allow state saving.
		 * @param seedIndex must be 0 or 1, designating which of the 2 long seeds to return
		 * @return the internal seed that can be used in setState
		 */
		[[nodiscard]] constexpr SeedType getState(const int seedIndex) const {
			return seedIndex == 0 ? seed0 : seed1;
		}
	};
}

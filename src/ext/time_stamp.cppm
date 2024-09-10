export module ext.time_stamp;

import std;
import ext.concepts;

export namespace ext {
	class [[deprecated]] TimeStamper {
		using StampID = unsigned int;
		using StdUnit = std::chrono::milliseconds;
		using Duration = std::chrono::duration<long long, std::milli>;
		std::unordered_map<StampID, std::chrono::time_point<std::chrono::system_clock>> stamps{};

	public:
		[[nodiscard]] std::size_t size() const {
			return stamps.size();
		}

		void mark(const StampID stampID = 0) {
			stamps.insert_or_assign(stampID, std::chrono::system_clock::now());
		}

		Duration popMark(const StampID stampID = 0) {
			const auto current = std::chrono::system_clock::now();
			auto&& begin = stamps.find(stampID);
			if(begin == stamps.end()) {
				return Duration(-1);
			}

			stamps.erase(begin);

			return std::chrono::duration_cast<StdUnit>(current - begin->second);
		}

		Duration toMark(const StampID stampID = 0) {
			const auto current = std::chrono::system_clock::now();
			auto&& begin = stamps.find(stampID);
			if(begin == stamps.end()) {
				return Duration(-1);
			}

			return std::chrono::duration_cast<StdUnit>(current - begin->second);
		}
	};

	template <std::size_t maxSize = 1, spec_of<std::chrono::duration> Duration = std::chrono::milliseconds>
	class array_time_stamper {
		std::array<std::chrono::time_point<std::chrono::system_clock>, maxSize> stamps{};

	public:
		using duration_type = Duration;
		using id_type = std::size_t;

		[[nodiscard]] static constexpr std::size_t size() noexcept{
			return maxSize;
		}

		template <id_type stampID = 0>
			requires (stampID < maxSize)
		void mark() noexcept{
			stamps.at(stampID) = std::chrono::system_clock::now();
		}


		template <id_type stampID = 0>
			requires (stampID < maxSize)
		[[nodiscard]] Duration get() const noexcept{
			const auto current = std::chrono::system_clock::now();

			return std::chrono::duration_cast<Duration>(current - stamps.at(stampID));
		}
	};
}

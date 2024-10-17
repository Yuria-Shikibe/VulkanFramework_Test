module;

export module Game.Faction;

import std;

import Graphic.Color;
import Graphic.ImageNineRegion;

export namespace Game {
	class Faction;

	using FactionID = unsigned int;

	namespace Global{
		inline std::unordered_map<FactionID, Faction> Factions{};
	}

	class Faction {
		FactionID id{};
		std::string name{};

	public:
		//TODO using a id -> relationship map instead?
		std::unordered_set<FactionID> ally{};
		std::unordered_set<FactionID> hostile{};
		// std::unordered_set<FactionID> neutral{};

		Graphic::Color color{};
		Graphic::ImageViewRegion icon{};

		[[nodiscard]] Faction() = default;

		[[nodiscard]] Faction(const FactionID id, const std::string_view name, const Graphic::Color color)
			: id(id),
			name(name),
			color(color) {
		}

		[[nodiscard]] explicit Faction(const FactionID id) noexcept
			: id(id) {
		}

		[[nodiscard]] bool isAllyTo(const FactionID id) const noexcept {
			return ally.contains(id);
		}

		[[nodiscard]] bool isHostileTo(const FactionID id) const noexcept {
			return hostile.contains(id);
		}

		[[nodiscard]] bool isNeutralTo(const FactionID id) const noexcept {
			return !isHostileTo(id) && !isAllyTo(id);
		}

		[[nodiscard]] FactionID getID() const noexcept{
			return id;
		}
	};

	class FactionData {

	};
}

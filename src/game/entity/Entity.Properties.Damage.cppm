//
// Created by Matrix on 2024/10/16.
//

export module Game.Entity.Properties.Damage;

export namespace Game{

	struct DamageProperty {
		float fullDamage;
		float pierceForce;

		float splashRadius;
		float splashAngle;

		[[nodiscard]] constexpr bool isSplashes() const noexcept{
			return splashRadius > 0;
		}

		[[nodiscard]] constexpr bool heal() const noexcept{
			return fullDamage < 0;
		}
	};

	struct DamageComposition {
		DamageProperty materialDamage; //Maybe basic damage
		DamageProperty fieldDamage; //Maybe real damage
		DamageProperty empDamage; //emp damage

		[[nodiscard]] constexpr float sum() const noexcept {
			return materialDamage.fullDamage + fieldDamage.fullDamage;
		}
	};
}

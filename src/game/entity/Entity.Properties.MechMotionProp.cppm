module;

#include <cassert>

export module Game.Entity.Properties.Motion;

export import Geom.Vector2D;
export import Geom.Transform;
export import Game.Entity.Unit;

import std;

export namespace Game{

	/**
	 * @brief MechanicalMotionProperty
	 */
	struct MechMotionProp{
		Geom::UniformTransform trans;
		Geom::UniformTransform vel;
		Geom::UniformTransform accel;

		constexpr void apply(const UpdateTick tick) noexcept{
			vel += accel * tick;
			trans += vel * tick;
		}

		constexpr void applyAndReset(const UpdateTick tick) noexcept{
			apply(tick);
			accel = {};
		}

		[[nodiscard]] constexpr Geom::Vec2 pos() const noexcept{
			return trans.vec;
		}

		void check() const noexcept{
			assert(!trans.vec.isNaN());
			assert(!vel.vec.isNaN());
			assert(!accel.vec.isNaN());

			assert(!std::isnan(static_cast<float>(trans.rot)));
			assert(!std::isnan(static_cast<float>(vel.rot)));
			assert(!std::isnan(static_cast<float>(accel.rot)));
		}
	};

	struct RigidProp{
		float inertialMass = 1000;
		float rotationalInertiaScale = 1 / 12.0f;

		/** @brief [0, 1]*/
		float frictionCoefficient = 0.35f;
		float restitution = 0.15f;

		/** @brief Used For Force Correction*/
		float collideForceScale = 1.0f;
	};
}

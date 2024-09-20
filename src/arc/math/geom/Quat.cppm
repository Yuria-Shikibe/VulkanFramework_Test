//
// Created by Matrix on 2024/8/22.
//

export module Geom.Quat;

import std;
import Math;
import Geom.Vector3D;

export namespace Geom {
    struct Quaternion {
        using T = float;
        using PassedVec3 = typename Vector3D<T>::PassType;

        T x{0.};
        T y{0.};
        T z{0.};
        T w{1.};

        /** @return the Euclidean length of the specified quaternion */
        static T len(const T x, const T y, const T z, const T w) noexcept {
            return Math::sqrt(x * x + y * y + z * z + w * w);
        }


        constexpr static T len2(const T x, const T y, const T z, const T w) noexcept {
            return x * x + y * y + z * z + w * w;
        }

        constexpr static T dot(const T x1, const T y1, const T z1, const T w1, const T x2, const T y2,
                               const T z2, const T w2) noexcept {
            return x1 * x2 + y1 * y2 + z1 * z2 + w1 * w2;
        }


        constexpr Quaternion &set(const T x, const T y, const T z, const T w) noexcept {
            this->x = x;
            this->y = y;
            this->z = z;
            this->w = w;
            return *this;
        }

        Quaternion &set(const Quaternion quat) noexcept {
            return this->set(quat.x, quat.y, quat.z, quat.w);
        }

        /**
         * Sets the quaternion components from the given axis and angle around that axis.
         * @param axis The axis
         * @param angle The angle in degrees
         * @return This quaternion for chaining.
         */

        Quaternion &set(const PassedVec3 axis, const T angle) noexcept {
            return setFromAxis(axis.x, axis.y, axis.z, angle);
        }

        /** @return a copy of this quaternion */

        Quaternion copy() {
            return {*this};
        }

        /** @return the Euclidean length of this quaternion */

        [[nodiscard]] T len() const {
            return Math::sqrt(x * x + y * y + z * z + w * w);
        }

        friend std::ostream &operator<<(std::ostream &os, const Quaternion &obj) {
            return os << "[" << obj.x << "|" << obj.y << "|" << obj.z << "|" << obj.w << "]";
        }

        /**
         * Sets the quaternion to the given euler angles in degrees.
         * @param yaw the rotation around the y-axis in degrees
         * @param pitch the rotation around the x-axis in degrees
         * @param roll the rotation around the z axis degrees
         * @return this quaternion
         */
        constexpr Quaternion &setEulerAngles(const T yaw, const T pitch, const T roll) noexcept {
            return setEulerAnglesRad(yaw * Math::DEGREES_TO_RADIANS, pitch * Math::DEGREES_TO_RADIANS, roll
                                     * Math::DEGREES_TO_RADIANS);
        }

        /**
         * Sets the quaternion to the given euler angles in radians.
         * @param yaw the rotation around the y-axis in radians
         * @param pitch the rotation around the x-axis in radians
         * @param roll the rotation around the z axis in radians
         * @return this quaternion
         */

        constexpr Quaternion &setEulerAnglesRad(const T yaw, const T pitch, const T roll) noexcept {
            const T hr = roll * 0.5f;
            const T shr = Math::sin(hr);
            const T chr = Math::cos(hr);
            const T hp = pitch * 0.5f;
            const T shp = Math::sin(hp);
            const T chp = Math::cos(hp);
            const T hy = yaw * 0.5f;
            const T shy = Math::sin(hy);
            const T chy = Math::cos(hy);
            const T chy_shp = chy * shp;
            const T shy_chp = shy * chp;
            const T chy_chp = chy * chp;
            const T shy_shp = shy * shp;

            x = (chy_shp * chr) + (shy_chp * shr);
            // cos(yaw/2) * sin(pitch/2) * cos(roll/2) + sin(yaw/2) * cos(pitch/2) * sin(roll/2)
            y = (shy_chp * chr) - (chy_shp * shr);
            // sin(yaw/2) * cos(pitch/2) * cos(roll/2) - cos(yaw/2) * sin(pitch/2) * sin(roll/2)
            z = (chy_chp * shr) - (shy_shp * chr);
            // cos(yaw/2) * cos(pitch/2) * sin(roll/2) - sin(yaw/2) * sin(pitch/2) * cos(roll/2)
            w = (chy_chp * chr) + (shy_shp * shr);
            // cos(yaw/2) * cos(pitch/2) * cos(roll/2) + sin(yaw/2) * sin(pitch/2) * sin(roll/2)
            return *this;
        }

        /**
         * Get the pole of the gimbal lock, if any.
         * @return positive (+1) for the North Pole, negative (-1) for the South Pole, zero (0) when no gimbal lock
         */
        [[nodiscard]] constexpr int getGimbalPole() const noexcept {
            const T t = y * x + z * w;
            return t > 0.499 ? 1 : (t < -0.499 ? -1 : 0);
        }

        /**
         * Get the roll euler angle in radians, which is the rotation around the z axis. Requires that this quaternion is normalized.
         * @return the rotation around the z axis in radians (between -PI and +PI)
         */
        [[nodiscard]] T getRollRad() const {
            const int pole = getGimbalPole();
            return pole == 0
                ? Math::atan2(1. - 2. * (x * x + z * z), 2. * (w * z + y * x))
                : pole * 2.
                * Math::atan2(w, y);
        }

        /**
         * Get the roll euler angle in degrees, which is the rotation around the z axis. Requires that this quaternion is normalized.
         * @return the rotation around the z axis in degrees (between -180 and +180)
         */

        T getRoll() {
            return getRollRad() * Math::RADIANS_TO_DEGREES;
        }

        /**
         * Get the pitch euler angle in radians, which is the rotation around the x-axis. Requires that this quaternion is normalized.
         * @return the rotation around the x-axis in radians (between -(PI/2) and +(PI/2))
         */

        [[nodiscard]] T getPitchRad() const noexcept {
            const int pole = getGimbalPole();
            return pole == 0 ? static_cast<T>(std::asin(Math::clamp(2. * (w * x - z * y), -1., 1.))) : pole * Math::PI * 0.5;
        }

        /**
         * Get the pitch euler angle in degrees, which is the rotation around the x-axis. Requires that this quaternion is normalized.
         * @return the rotation around the x-axis in degrees (between -90 and +90)
         */

        [[nodiscard]] T getPitch() const noexcept  {
            return getPitchRad() * Math::RADIANS_TO_DEGREES;
        }

        /**
         * Get the yaw euler angle in radians, which is the rotation around the y-axis. Requires that this quaternion is normalized.
         * @return the rotation around the y-axis in radians (between -PI and +PI)
         */

        [[nodiscard]] T getYawRad() const noexcept {
            return getGimbalPole() == 0 ? Math::atan2(1. - 2. * (y * y + x * x), 2. * (y * w + x * z)) : 0.;
        }

        /**
         * Get the yaw euler angle in degrees, which is the rotation around the y-axis. Requires that this quaternion is normalized.
         * @return the rotation around the y-axis in degrees (between -180 and +180)
         */

        [[nodiscard]] T getYaw() const noexcept {
            return getYawRad() * Math::RADIANS_TO_DEGREES;
        }

        /** @return the length of this quaternion without square root */

        [[nodiscard]] constexpr T len2() const noexcept {
            return x * x + y * y + z * z + w * w;
        }

        /**
         * Normalizes this quaternion to unit length
         * @return the quaternion for chaining
         */
        Quaternion& nor() {
            T len = len2();
            if(len != 0.f && !Math::equal(len, 1.)) {
                len = Math::sqrt(len);
                w /= len;
                x /= len;
                y /= len;
                z /= len;
            }
            return *this;
        }

        // TODO : this would better fit into the Vec3 class

        /**
         * Conjugate the quaternion.
         * @return This quaternion for chaining
         */

        constexpr Quaternion& conjugate() noexcept {
            x = -x;
            y = -y;
            z = -z;
            return *this;
        }

        /**
         * Transforms the given vector using this quaternion
         * @param v Vector to transform
         */
        Vector3D<T>& transform(Vector3D<T>& v) const {
            Quaternion tmp1{};
            Quaternion tmp2{*this};

            tmp2.conjugate();
            tmp2.mulLeft(tmp1.set(v.x, v.y, v.z, 0)).mulLeft(*this);

            v.x = tmp2.x;
            v.y = tmp2.y;
            v.z = tmp2.z;
            return v;
        }

        /**
         * Multiplies this quaternion with another one in the form of this = this * other
         * @param other Quaternion to multiply with
         * @return This quaternion for chaining
         */
        constexpr Quaternion& mul(const Quaternion& other) noexcept {
            const T newX = this->w * other.x + this->x * other.w + this->y * other.z - this->z * other.y;
            const T newY = this->w * other.y + this->y * other.w + this->z * other.x - this->x * other.z;
            const T newZ = this->w * other.z + this->z * other.w + this->x * other.y - this->y * other.x;
            const T newW = this->w * other.w - this->x * other.x - this->y * other.y - this->z * other.z;
            this->x = newX;
            this->y = newY;
            this->z = newZ;
            this->w = newW;
            return *this;
        }

        constexpr Quaternion& mul(const T x, const T y, const T z, const T w) noexcept{
            const T newX = this->w * x + this->x * w + this->y * z - this->z * y;
            const T newY = this->w * y + this->y * w + this->z * x - this->x * z;
            const T newZ = this->w * z + this->z * w + this->x * y - this->y * x;
            const T newW = this->w * w - this->x * x - this->y * y - this->z * z;
            this->x = newX;
            this->y = newY;
            this->z = newZ;
            this->w = newW;
            return *this;
        }

        /**
         * Multiplies this quaternion with another one in the form of this = other * this
         * @param other Quaternion to multiply with
         * @return This quaternion for chaining
         */

        constexpr Quaternion& mulLeft(const Quaternion& other) noexcept {
            const T newX = other.w * this->x + other.x * this->w + other.y * this->z - other.z * this->y;
            const T newY = other.w * this->y + other.y * this->w + other.z * this->x - other.x * this->z;
            const T newZ = other.w * this->z + other.z * this->w + other.x * this->y - other.y * this->x;
            const T newW = other.w * this->w - other.x * this->x - other.y * this->y - other.z * this->z;
            this->x = newX;
            this->y = newY;
            this->z = newZ;
            this->w = newW;
            return *this;
        }

        /**
         * Multiplies this quaternion with another one in the form of this = other * this
         * @param x the x component of the other quaternion to multiply with
         * @param y the y component of the other quaternion to multiply with
         * @param z the z component of the other quaternion to multiply with
         * @param w the w component of the other quaternion to multiply with
         * @return This quaternion for chaining
         */

        constexpr Quaternion& mulLeft(const T x, const T y, const T z, const T w) noexcept {
            const T newX = w * this->x + x * this->w + y * this->z - z * this->y;
            const T newY = w * this->y + y * this->w + z * this->x - x * this->z;
            const T newZ = w * this->z + z * this->w + x * this->y - y * this->x;
            const T newW = w * this->w - x * this->x - y * this->y - z * this->z;
            this->x = newX;
            this->y = newY;
            this->z = newZ;
            this->w = newW;
            return *this;
        }

        /** Add the x,y,z,w components of the passed in quaternion to the ones of this quaternion */

        constexpr Quaternion& add(const Quaternion& quat) noexcept {
            this->x += quat.x;
            this->y += quat.y;
            this->z += quat.z;
            this->w += quat.w;
            return *this;
        }

        /** Add the x,y,z,w components of the passed in quaternion to the ones of this quaternion */

        constexpr Quaternion& add(const T qx, const T qy, const T qz, const T qw) noexcept {
            this->x += qx;
            this->y += qy;
            this->z += qz;
            this->w += qw;
            return *this;
        }

        /**
         * Sets the quaternion to an identity Quaternion
         * @return this quaternion for chaining
         */

        constexpr Quaternion& idt() {
            return this->set(0, 0, 0, 1);
        }

        /** @return If this quaternion is an identity Quaternion */

        [[nodiscard]] bool isIdentity() const {
            return Math::zero(x) && Math::zero(y) && Math::zero(z) && Math::equal(w, 1.);
        }

        // todo : the setFromAxis(v3,T) method should replace the set(v3,T) method

        /** @return If this quaternion is an identity Quaternion */

        [[nodiscard]] bool isIdentity(const T tolerance) const {
            return Math::zero(x, tolerance) && Math::zero(y, tolerance) && Math::zero(z, tolerance)
                && Math::equal(w, 1., tolerance);
        }

        /**
         * Sets the quaternion components from the given axis and angle around that axis.
         * @param axis The axis
         * @param degrees The angle in degrees
         * @return This quaternion for chaining.
         */

        Quaternion& setFromAxis(const PassedVec3 axis, const T degrees) noexcept {
            return setFromAxis(axis.x, axis.y, axis.z, degrees);
        }

        /**
         * Sets the quaternion components from the given axis and angle around that axis.
         * @param axis The axis
         * @param radians The angle in radians
         * @return This quaternion for chaining.
         */

        Quaternion& setFromAxisRad(const PassedVec3 axis, const T radians) noexcept {
            return setFromAxisRad(axis.x, axis.y, axis.z, radians);
        }

        /**
         * Sets the quaternion components from the given axis and angle around that axis.
         * @param x X direction of the axis
         * @param y Y direction of the axis
         * @param z Z direction of the axis
         * @param degrees The angle in degrees
         * @return This quaternion for chaining.
         */

        Quaternion& setFromAxis(const T x, const T y, const T z, const T degrees) noexcept {
            return setFromAxisRad(x, y, z, degrees * Math::DEGREES_TO_RADIANS);
        }

        /**
         * Sets the quaternion components from the given axis and angle around that axis.
         * @param x X direction of the axis
         * @param y Y direction of the axis
         * @param z Z direction of the axis
         * @param radians The angle in radians
         * @return This quaternion for chaining.
         */

        Quaternion& setFromAxisRad(const T x, const T y, const T z, const T radians) noexcept {
            T d = Vector3D<T>::len(x, y, z);
            if(d == 0.)
                return idt();
            d = static_cast<T>(1.) / d;
            const T l_ang = radians < 0 ? Math::PI2 - Math::mod(-radians, Math::PI2) : Math::mod(radians, Math::PI2);
            const T l_sin = Math::sin(l_ang / 2);
            const T l_cos = Math::cos(l_ang / 2);
            return this->set(d * x * l_sin, d * y * l_sin, d * z * l_sin, l_cos).nor();
        }

     
        /**
         * <p>
         * Sets the Quaternion from the given x-, y- and z-axis which have to be orthonormal.
         * </p>
         *
         * <p>
         * Taken from Bones framework for JPCT, see http://www.aptalkarga.com/bones/ which in turn took it from Graphics Gem code at
         * ftp://ftp.cis.upenn.edu/pub/graphics/shoemake/quatut.ps.Z.
         * </p>
         * @param xx x-axis x-coordinate
         * @param xy x-axis y-coordinate
         * @param xz x-axis z-coordinate
         * @param yx y-axis x-coordinate
         * @param yy y-axis y-coordinate
         * @param yz y-axis z-coordinate
         * @param zx z-axis x-coordinate
         * @param zy z-axis y-coordinate
         * @param zz z-axis z-coordinate
         */
        Quaternion& setFromAxes(const T xx, const T xy, const T xz, const T yx, const T yy, const T yz, const T zx, const T zy, const T zz) noexcept {
            return setFromAxes(false, xx, xy, xz, yx, yy, yz, zx, zy, zz);
        }

        /**
         * <p>
         * Sets the Quaternion from the given x-, y- and z-axis.
         * </p>
         *
         * <p>
         * Taken from Bones framework for JPCT, see http://www.aptalkarga.com/bones/ which in turn took it from Graphics Gem code at
         * ftp://ftp.cis.upenn.edu/pub/graphics/shoemake/quatut.ps.Z.
         * </p>
         * @param normalizeAxes whether to normalize the axes (necessary when they contain scaling)
         * @param xx x-axis x-coordinate
         * @param xy x-axis y-coordinate
         * @param xz x-axis z-coordinate
         * @param yx y-axis x-coordinate
         * @param yy y-axis y-coordinate
         * @param yz y-axis z-coordinate
         * @param zx z-axis x-coordinate
         * @param zy z-axis y-coordinate
         * @param zz z-axis z-coordinate
         */
        Quaternion& setFromAxes(const bool normalizeAxes, T xx, T xy, T xz, T yx, T yy, T yz, T zx,
                         T zy, T zz) noexcept {
            if(normalizeAxes) {
                const T lx = 1. / Vector3D<T>::len(xx, xy, xz);
                const T ly = 1. / Vector3D<T>::len(yx, yy, yz);
                const T lz = 1. / Vector3D<T>::len(zx, zy, zz);
                xx *= lx;
                xy *= lx;
                xz *= lx;
                yx *= ly;
                yy *= ly;
                yz *= ly;
                zx *= lz;
                zy *= lz;
                zz *= lz;
            }
            // the trace is the sum of the diagonal elements; see
            // http://mathworld.wolfram.com/MatrixTrace.html
            const T t = xx + yy + zz;

            // we protect the division by s by ensuring that s>=1
            if(t >= 0) {
                // |w| >= .5
                T s = Math::sqrt(t + 1); // |s|>=1 ...
                w = 0.5f * s;
                s = 0.5f / s; // so this division isn't bad
                x = (zy - yz) * s;
                y = (xz - zx) * s;
                z = (yx - xy) * s;
            }
            else if((xx > yy) && (xx > zz)) {
                T s = Math::sqrt(1.0 + xx - yy - zz); // |s|>=1
                x = s * 0.5f;                            // |x| >= .5
                s = 0.5f / s;
                y = (yx + xy) * s;
                z = (xz + zx) * s;
                w = (zy - yz) * s;
            }
            else if(yy > zz) {
                T s = Math::sqrt(1.0 + yy - xx - zz); // |s|>=1
                y = s * 0.5f;                            // |y| >= .5
                s = 0.5f / s;
                x = (yx + xy) * s;
                z = (zy + yz) * s;
                w = (xz - zx) * s;
            }
            else {
                T s = Math::sqrt(1.0 + zz - xx - yy); // |s|>=1
                z = s * 0.5f;                            // |z| >= .5
                s = 0.5f / s;
                x = (xz + zx) * s;
                y = (zy + yz) * s;
                w = (yx - xy) * s;
            }

            return *this;
        }

        /**
         * Set this quaternion to the rotation between two vectors.
         * @param v1 The base vector, which should be normalized.
         * @param v2 The target vector, which should be normalized.
         * @return This quaternion for chaining
         */

        Quaternion& setFromCross(const PassedVec3 v1, const PassedVec3 v2) {
            const T dot = Math::clamp(v1.dot(v2), static_cast<T>(-1.), static_cast<T>(1.));
            const T angle = (T)std::acos(dot);
            return setFromAxisRad(v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x, angle);
        }

        /**
         * Set this quaternion to the rotation between two vectors.
         * @param x1 The base vectors x value, which should be normalized.
         * @param y1 The base vectors y value, which should be normalized.
         * @param z1 The base vectors z value, which should be normalized.
         * @param x2 The target vector x value, which should be normalized.
         * @param y2 The target vector y value, which should be normalized.
         * @param z2 The target vector z value, which should be normalized.
         * @return This quaternion for chaining
         */

        Quaternion& setFromCross(const T x1, const T y1, const T z1, const T x2, const T y2, const T z2) {
            const T dot = Math::clamp(Vector3D<T>::dot(x1, y1, z1, x2, y2, z2), static_cast<T>(-1.), static_cast<T>(1.));
            const T angle = (T)std::acos(dot);
            return setFromAxisRad(y1 * z2 - z1 * y2, z1 * x2 - x1 * z2, x1 * y2 - y1 * x2, angle);
        }

        /**
         * Spherical Linear interpolation between this quaternion and the other quaternion, based on the alpha value in the range
         * [0,1]. Taken from Bones framework for JPCT, see http://www.aptalkarga.com/bones/
         * @param end the end quaternion
         * @param alpha alpha in the range [0,1]
         * @return this quaternion for chaining
         */

        Quaternion& slerp(const Quaternion end, T alpha) {
            const T d = this->x * end.x + this->y * end.y + this->z * end.z + this->w * end.w;
            T absDot = d < 0.f ? -d : d;

            // Set the first and second scale for the interpolation
            T scale0 = static_cast<T>(1.) - alpha;
            T scale1 = alpha;

            // Check if the angle between the 2 quaternions was big enough to
            // warrant such calculations
            if((1 - absDot) > 0.1) {
                // Get the angle between the 2 quaternions,
                // and then store the sin() of that angle
                const T angle = (T)std::acos(absDot);
                const T invSinTheta = 1. / Math::sin(angle);

                // Calculate the scale for q1 and q2, according to the angle and
                // it's sine value
                scale0 = (Math::sin((1. - alpha) * angle) * invSinTheta);
                scale1 = (Math::sin(alpha * angle) * invSinTheta);
            }

            if(d < 0.f)
                scale1 = -scale1;

            // Calculate the x, y, z and w values for the quaternion by using a
            // special form of Linear interpolation for quaternions.
            x = (scale0 * x) + (scale1 * end.x);
            y = (scale0 * y) + (scale1 * end.y);
            z = (scale0 * z) + (scale1 * end.z);
            w = (scale0 * w) + (scale1 * end.w);

            // Return the interpolated quaternion
            return *this;
        }

        /**
         * Spherical linearly interpolates multiple quaternions and stores the result in this Quaternion. Will not destroy the data
         * previously inside the elements of q. result = (q_1^w_1)*(q_2^w_2)* ... *(q_n^w_n) where w_i=1/n.
         * @param q List of quaternions
         * @return This quaternion for chaining
         */
        Quaternion& slerp(std::span<Quaternion> q) {
            Quaternion tmp1{};

            // Calculate exponents and multiply everything from left to right
            const T w = 1.0f / q.size();
            set(q[0]).exp(w);
            for(int i = 1; i < q.size(); i++)
                mul(tmp1.set(q[i]).exp(w));
            nor();
            return *this;
        }

        /**
         * Spherical linearly interpolates multiple quaternions by the given weights and stores the result in this Quaternion. Will not
         * destroy the data previously inside the elements of q or w. result = (q_1^w_1)*(q_2^w_2)* ... *(q_n^w_n) where the sum of w_i
         * is 1. Lists must be equal in length.
         * @param q List of quaternions
         * @param w List of weights
         * @return This quaternion for chaining
         */

        Quaternion& slerp(std::span<Quaternion> q, std::span<T> w) {
            Quaternion tmp1{};

            // Calculate exponents and multiply everything from left to right
            set(q[0]).exp(w[0]);
            for(int i = 1; i < q.size(); i++)
                mul(tmp1.set(q[i]).exp(w[i]));
            nor();
            return *this;
        }

        /**
         * Calculates (this quaternion)^alpha where alpha is a real number and stores the result in this quaternion. See
         * http://en.wikipedia.org/wiki/Quaternion#Exponential.2C_logarithm.2C_and_power
         * @param alpha Exponent
         * @return This quaternion for chaining
         */

        Quaternion& exp(T alpha) {

            // Calculate |q|^alpha
            T norm = len();
            T normExp = (T)std::pow(norm, alpha);

            // Calculate theta
            T theta = (T)std::acos(w / norm);

            // Calculate coefficient of basis elements
            T coeff = 0;
            if(Math::abs(theta) < 0.001)
                // If theta is small enough, use the limit of sin(alpha*theta) / sin(theta) instead of actual
                // value
                coeff = normExp * alpha / norm;
            else
                coeff = normExp * Math::sin(alpha * theta) / (norm * Math::sin(theta));

            // Write results
            w = normExp * Math::cos(alpha * theta);
            x *= coeff;
            y *= coeff;
            z *= coeff;

            // Fix any possible discrepancies
            nor();

            return *this;
        }

        friend bool operator==(const Quaternion&, const Quaternion&) = default;

        [[nodiscard]] T dot(const Quaternion& other) const {
            return this->x * other.x + this->y * other.y + this->z * other.z + this->w * other.w;
        }

        [[nodiscard]] T dot(const T x, const T y, const T z, const T w) const {
            return this->x * x + this->y * y + this->z * z + this->w * w;
        }

        constexpr Quaternion& mul(const T scalar) noexcept {
            this->x *= scalar;
            this->y *= scalar;
            this->z *= scalar;
            this->w *= scalar;
            return *this;
        }

        /**
         * Get the axis angle representation of the rotation in degrees. The supplied vector will receive the axis (x, y and z values)
         * of the rotation and the value returned is the angle in degrees around that axis. Note that this method will alter the
         * supplied vector, the existing value of the vector is ignored. </p> This will normalize this quaternion if needed. The
         * received axis is a unit vector. However, if this is an identity quaternion (no rotation), then the length of the axis may be
         * zero.
         * @param axis vector which will receive the axis
         * @return the angle in degrees
         * @see <a href="http://en.wikipedia.org/wiki/Axis%E2%80%93angle_representation">wikipedia</a>
         * @see <a href="http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToAngle">calculation</a>
         */

        T getAxisAngle(const PassedVec3 axis) {
            return getAxisAngleRad(axis) * Math::RADIANS_TO_DEGREES;
        }

        /**
         * Get the axis-angle representation of the rotation in radians. The supplied vector will receive the axis (x, y and z values)
         * of the rotation and the value returned is the angle in radians around that axis. Note that this method will alter the
         * supplied vector, the existing value of the vector is ignored. </p> This will normalize this quaternion if needed. The
         * received axis is a unit vector. However, if this is an identity quaternion (no rotation), then the length of the axis may be
         * zero.
         * @param axis vector which will receive the axis
         * @return the angle in radians
         * @see <a href="http://en.wikipedia.org/wiki/Axis%E2%80%93angle_representation">wikipedia</a>
         * @see <a href="http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToAngle">calculation</a>
         */

        T getAxisAngleRad(PassedVec3 axis) {
            if(this->w > 1)
                this->nor(); // if w>1 acos and sqrt will produce errors, this cant happen if quaternion is normalised
            const T angle = static_cast<T>(2.0 * std::acos(this->w));
            auto s = Math::sqrt(1 - this->w * this->w);
            // assuming quaternion normalised then w is less than 1, so term always positive.
            if(s < Math::FLOAT_ROUNDING_ERROR) {
                // test to avoid divide by zero, s is always positive due to sqrt
                // if s close to zero then direction of axis not important
                axis.x = this->x; // if it is important that axis is normalised then replace with x=1; y=z=0;
                axis.y = this->y;
                axis.z = this->z;
            }
            else {
                axis.x = this->x / s; // normalise axis
                axis.y = this->y / s;
                axis.z = this->z / s;
            }

            return angle;
        }

        /**
         * Get the angle in radians of the rotation this quaternion represents. Does not normalize the quaternion. Use
         * {@link #getAxisAngleRad(Vec3)} to get both the axis and the angle of this rotation. Use
         * {@link #getAngleAroundRad(Vec3)} to get the angle around a specific axis.
         * @return the angle in radians of the rotation
         */

        T getAngleRad() {
            return static_cast<T>(2.0 * std::acos((this->w > 1) ? (this->w / len()) : this->w));
        }

        /**
         * Get the angle in degrees of the rotation this quaternion represents. Use {@link #getAxisAngle(Vec3)} to get both the axis
         * and the angle of this rotation. Use {@link #getAngleAround(Vec3)} to get the angle around a specific axis.
         * @return the angle in degrees of the rotation
         */
        T getAngle() {
            return getAngleRad() * Math::RADIANS_TO_DEGREES;
        }

        /**
         * Get the swing rotation and twist rotation for the specified axis. The twist rotation represents the rotation around the
         * specified axis. The swing rotation represents the rotation of the specified axis itself, which is the rotation around an
         * axis perpendicular to the specified axis. </p> The swing and twist rotation can be used to reconstruct the original
         * quaternion: this = swing * twist
         * @param axisX the X component of the normalized axis for which to get the swing and twist rotation
         * @param axisY the Y component of the normalized axis for which to get the swing and twist rotation
         * @param axisZ the Z component of the normalized axis for which to get the swing and twist rotation
         * @param swing will receive the swing rotation: the rotation around an axis perpendicular to the specified axis
         * @param twist will receive the twist rotation: the rotation around the specified axis
         * @see <a href="http://www.euclideanspace.com/maths/geometry/rotations/for/decomposition">calculation</a>
         */

        void getSwingTwist(const T axisX, const T axisY, const T axisZ, Quaternion& swing,
        Quaternion& twist) {
            const T d = Vector3D<T>::dot(this->x, this->y, this->z, axisX, axisY, axisZ);
            twist.set(axisX * d, axisY * d, axisZ * d, this->w).nor();
            if(d < 0)
                twist.mul(-1.);
            swing.set(twist).conjugate().mulLeft(*this);
        }

        /**
         * Get the swing rotation and twist rotation for the specified axis. The twist rotation represents the rotation around the
         * specified axis. The swing rotation represents the rotation of the specified axis itself, which is the rotation around an
         * axis perpendicular to the specified axis. </p> The swing and twist rotation can be used to reconstruct the original
         * quaternion: this = swing * twist
         * @param axis the normalized axis for which to get the swing and twist rotation
         * @param swing will receive the swing rotation: the rotation around an axis perpendicular to the specified axis
         * @param twist will receive the twist rotation: the rotation around the specified axis
         * @see <a href="http://www.euclideanspace.com/maths/geometry/rotations/for/decomposition">calculation</a>
         */

        void getSwingTwist(const PassedVec3 axis, Quaternion& swing, Quaternion twist) {
            getSwingTwist(axis.x, axis.y, axis.z, swing, twist);
        }

        /**
         * Get the angle in radians of the rotation around the specified axis. The axis must be normalized.
         * @param axisX the x component of the normalized axis for which to get the angle
         * @param axisY the y component of the normalized axis for which to get the angle
         * @param axisZ the z component of the normalized axis for which to get the angle
         * @return the angle in radians of the rotation around the specified axis
         */

        [[nodiscard]] T getAngleAroundRad(const T axisX, const T axisY, const T axisZ) const noexcept {
            const T d = Vector3D<T>::dot(this->x, this->y, this->z, axisX, axisY, axisZ);
            const T l2 = len2(axisX * d, axisY * d, axisZ * d, this->w);
            return Math::zero(l2)
                ? 0.
                : static_cast<T>(2.0 * std::acos(Math::clamp(
                    (d < 0 ? -this->w : this->w) / Math::sqrt(l2), static_cast<T>(-1.), static_cast<T>(1.))));
        }

        /**
         * Get the angle in radians of the rotation around the specified axis. The axis must be normalized.
         * @param axis the normalized axis for which to get the angle
         * @return the angle in radians of the rotation around the specified axis
         */

        [[nodiscard]] T getAngleAroundRad(const PassedVec3 axis) const noexcept {
            return getAngleAroundRad(axis.x, axis.y, axis.z);
        }

     
        [[nodiscard]] T getAngleAround(const T axisX, const T axisY, const T axisZ) const noexcept {
            return getAngleAroundRad(axisX, axisY, axisZ) * Math::RADIANS_TO_DEGREES;
        }

        /**
         * Get the angle in degrees of the rotation around the specified axis. The axis must be normalized.
         * @param axis the normalized axis for which to get the angle
         * @return the angle in degrees of the rotation around the specified axis
         */

        [[nodiscard]] T getAngleAround(const PassedVec3 axis) const noexcept {
            return getAngleAround(axis.x, axis.y, axis.z);
        }
    };

    using Quat = Quaternion;
}

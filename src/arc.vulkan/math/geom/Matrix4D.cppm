//
// Created by Matrix on 2024/8/22.
//

export module Geom.Matrix4D;

import std;
import Math;
export import Geom.Vector3D;
export import Geom.Quat;

export namespace Geom {
    struct Matrix4D {
        /**
         * XX: Typically the unrotated X component for scaling, also the cosine of the angle when rotated on the Y and/or Z axis. On
         * Vec3 multiplication this value is multiplied with the source X component and added to the target X component.
         */
        static constexpr int M00 = 0;

        /**
         * XY: Typically the negative sine of the angle when rotated on the Z axis. On Vec3 multiplication this value is multiplied
         * with the source Y component and added to the target X component.
         */
        static constexpr int M01 = 4;

        /**
         * XZ: Typically the sine of the angle when rotated on the Y axis. On Vec3 multiplication this value is multiplied with the
         * source Z component and added to the target X component.
         */
        static constexpr int M02 = 8;

        /** XW: Typically the translation of the X component. On Vec3 multiplication this value is added to the target X component. */
        static constexpr int M03 = 12;

        /**
         * YX: Typically the sine of the angle when rotated on the Z axis. On Vec3 multiplication this value is multiplied with the
         * source X component and added to the target Y component.
         */
        static constexpr int M10 = 1;

        /**
         * YY: Typically the unrotated Y component for scaling, also the cosine of the angle when rotated on the X and/or Z axis. On
         * Vec3 multiplication this value is multiplied with the source Y component and added to the target Y component.
         */
        static constexpr int M11 = 5;

        /**
         * YZ: Typically the negative sine of the angle when rotated on the X axis. On Vec3 multiplication this value is multiplied
         * with the source Z component and added to the target Y component.
         */
        static constexpr int M12 = 9;

        /** YW: Typically the translation of the Y component. On Vec3 multiplication this value is added to the target Y component. */
        static constexpr int M13 = 13;

        /**
         * ZX: Typically the negative sine of the angle when rotated on the Y axis. On Vec3 multiplication this value is multiplied
         * with the source X component and added to the target Z component.
         */
        static constexpr int M20 = 2;

        /**
         * ZY: Typical the sine of the angle when rotated on the X axis. On Vec3 multiplication this value is multiplied with the
         * source Y component and added to the target Z component.
         */
        static constexpr int M21 = 6;

        /**
         * ZZ: Typically the unrotated Z component for scaling, also the cosine of the angle when rotated on the X and/or Y axis. On
         * Vec3 multiplication this value is multiplied with the source Z component and added to the target Z component.
         */
        static constexpr int M22 = 10;

        /** ZW: Typically the translation of the Z component. On Vec3 multiplication this value is added to the target Z component. */
        static constexpr int M23 = 14;

        /** WX: Typically the value zero. On Vec3 multiplication this value is ignored. */
        static constexpr int M30 = 3;

        /** WY: Typically the value zero. On Vec3 multiplication this value is ignored. */
        static constexpr int M31 = 7;

        /** WZ: Typically the value zero. On Vec3 multiplication this value is ignored. */
        static constexpr int M32 = 11;

        /** WW: Typically the value one. On Vec3 multiplication this value is ignored. */
        static constexpr int M33 = 15;

        using T = float;
        using MatRaw = std::array<float, 16>;
        using PassedVec3 = typename Vector3D<T>::PassType;
        using PassedQuat = Geom::Quat;

    private:
        MatRaw val{};

    public:
        /** Constructs an identity matrix */

        [[nodiscard]] constexpr Matrix4D() noexcept {
            val[M00] = 1;
            val[M11] = 1;
            val[M22] = 1;
            val[M33] = 1;
        }


        [[nodiscard]] constexpr Matrix4D(const MatRaw &values) noexcept {
            set(values);
        }

        constexpr Matrix4D &set(const Matrix4D &matrix) noexcept {
            return this->operator=(matrix);
        }

        constexpr Matrix4D &set(const MatRaw &values) noexcept {
            val = values;
            return *this;
        }

        constexpr Matrix4D &set(const T quaternionX, const T quaternionY, const T quaternionZ, const T quaternionW) noexcept {
            return set(0, 0, 0, quaternionX, quaternionY, quaternionZ, quaternionW);
        }


        /**
         * Sets the matrix to a rotation matrix representing the translation and quaternion.
         * @param translationX The X component of the translation that is to be used to set this matrix.
         * @param translationY The Y component of the translation that is to be used to set this matrix.
         * @param translationZ The Z component of the translation that is to be used to set this matrix.
         * @param quaternionX The X component of the quaternion that is to be used to set this matrix.
         * @param quaternionY The Y component of the quaternion that is to be used to set this matrix.
         * @param quaternionZ The Z component of the quaternion that is to be used to set this matrix.
         * @param quaternionW The W component of the quaternion that is to be used to set this matrix.
         * @return This matrix for the purpose of chaining methods together.
         */
        constexpr Matrix4D &set(const T translationX, const T translationY, const T translationZ, const T quaternionX,
                             const T quaternionY,
                             const T quaternionZ, const T quaternionW) noexcept {
            const T xs = quaternionX * 2, ys = quaternionY * 2, zs = quaternionZ * 2;
            const T wx = quaternionW * xs, wy = quaternionW * ys, wz = quaternionW * zs;
            const T xx = quaternionX * xs, xy = quaternionX * ys, xz = quaternionX * zs;
            const T yy = quaternionY * ys, yz = quaternionY * zs, zz = quaternionZ * zs;

            val[M00] = (1.0f - (yy + zz));
            val[M01] = (xy - wz);
            val[M02] = (xz + wy);
            val[M03] = translationX;

            val[M10] = (xy + wz);
            val[M11] = (1.0f - (xx + zz));
            val[M12] = (yz - wx);
            val[M13] = translationY;

            val[M20] = (xz - wy);
            val[M21] = (yz + wx);
            val[M22] = (1.0f - (xx + yy));
            val[M23] = translationZ;

            val[M30] = 0.f;
            val[M31] = 0.f;
            val[M32] = 0.f;
            val[M33] = 1.0f;
            return *this;
        }

        /**
         * Set this matrix to the specified translation, rotation and scale.
         * @param position The translation
         * @param orientation The rotation, must be normalized
         * @param scale The scale
         * @return This matrix for chaining
         */
        auto& set(const PassedVec3 position, const PassedQuat orientation, const PassedVec3 scale) {
            return set(position.x, position.y, position.z, orientation.x, orientation.y, orientation.z, orientation.w, scale.x,
                       scale.y, scale.z);
        }

        /**
         * Sets the matrix to a rotation matrix representing the translation and quaternion.
         * @param translationX The X component of the translation that is to be used to set this matrix.
         * @param translationY The Y component of the translation that is to be used to set this matrix.
         * @param translationZ The Z component of the translation that is to be used to set this matrix.
         * @param quaternionX The X component of the quaternion that is to be used to set this matrix.
         * @param quaternionY The Y component of the quaternion that is to be used to set this matrix.
         * @param quaternionZ The Z component of the quaternion that is to be used to set this matrix.
         * @param quaternionW The W component of the quaternion that is to be used to set this matrix.
         * @param scaleX The X component of the scaling that is to be used to set this matrix.
         * @param scaleY The Y component of the scaling that is to be used to set this matrix.
         * @param scaleZ The Z component of the scaling that is to be used to set this matrix.
         * @return This matrix for the purpose of chaining methods together.
         */
        constexpr Matrix4D &set(
            const T translationX, const T translationY, const T translationZ,
            const T quaternionX, const T quaternionY, const T quaternionZ, const T quaternionW,
            const T scaleX, const T scaleY, const T scaleZ
            ) noexcept {
            const T xs = quaternionX * 2, ys = quaternionY * 2, zs = quaternionZ * 2;
            const T wx = quaternionW * xs, wy = quaternionW * ys, wz = quaternionW * zs;
            const T xx = quaternionX * xs, xy = quaternionX * ys, xz = quaternionX * zs;
            const T yy = quaternionY * ys, yz = quaternionY * zs, zz = quaternionZ * zs;

            val[M00] = scaleX * (1.0f - (yy + zz));
            val[M01] = scaleY * (xy - wz);
            val[M02] = scaleZ * (xz + wy);
            val[M03] = translationX;

            val[M10] = scaleX * (xy + wz);
            val[M11] = scaleY * (1.0f - (xx + zz));
            val[M12] = scaleZ * (yz - wx);
            val[M13] = translationY;

            val[M20] = scaleX * (xz - wy);
            val[M21] = scaleY * (yz + wx);
            val[M22] = scaleZ * (1.0f - (xx + yy));
            val[M23] = translationZ;

            val[M30] = 0.f;
            val[M31] = 0.f;
            val[M32] = 0.f;
            val[M33] = 1.0f;
            return *this;
        }

        auto& set(PassedVec3 position, const PassedQuat& orientation, PassedVec3 scale){
            return set(position.x, position.y, position.z, orientation.x, orientation.y, orientation.z, orientation.w, scale.x,
            scale.y, scale.z);
        }

        /**
         * Sets the four columns of the matrix which correspond to the x-, y- and z-axis of the vector space this matrix creates as
         * well as the 4th column representing the translation of any point that is multiplied by this matrix.
         * @param xAxis The x-axis.
         * @param yAxis The y-axis.
         * @param zAxis The z-axis.
         * @param pos The translation vector.
         */

        constexpr Matrix4D &set(
            const PassedVec3 xAxis,
            const PassedVec3 yAxis,
            const PassedVec3 zAxis,
            const PassedVec3 pos
            ) noexcept {
            val[M00] = xAxis.x;
            val[M01] = xAxis.y;
            val[M02] = xAxis.z;
            val[M10] = yAxis.x;
            val[M11] = yAxis.y;
            val[M12] = yAxis.z;
            val[M20] = zAxis.x;
            val[M21] = zAxis.y;
            val[M22] = zAxis.z;
            val[M03] = pos.x;
            val[M13] = pos.y;
            val[M23] = pos.z;
            val[M30] = 0;
            val[M31] = 0;
            val[M32] = 0;
            val[M33] = 1;

            return *this;
        }

        [[nodiscard]] constexpr T operator[](const std::size_t index) const noexcept {
            return val[index];
        }

        friend Vector3D<T> operator*(const PassedVec3 vec, const Matrix4D& matrix4D) {
            Vector3D<T> rst{};

            rst.x = vec.x * matrix4D[M00] + vec.y * matrix4D[M01] + vec.z * matrix4D[M02];
            rst.y = vec.x * matrix4D[M10] + vec.y * matrix4D[M11] + vec.z * matrix4D[M12];
            rst.z = vec.x * matrix4D[M20] + vec.y * matrix4D[M21] + vec.z * matrix4D[M22];
            rst.add(matrix4D[M30], matrix4D[M31], matrix4D[M32]);

            return rst;
        }

        friend Vector3D<T>& operator*=(Vector3D<T>& vec, const Matrix4D& matrix4D) {
            Vector3D<T> rst{};

            rst.x = vec.x * matrix4D[M00] + vec.y * matrix4D[M01] + vec.z * matrix4D[M02];
            rst.y = vec.x * matrix4D[M10] + vec.y * matrix4D[M11] + vec.z * matrix4D[M12];
            rst.z = vec.x * matrix4D[M20] + vec.y * matrix4D[M21] + vec.z * matrix4D[M22];
            rst.add(matrix4D[M03], matrix4D[M13], matrix4D[M23]);

            return vec.set(rst);
        }

        [[nodiscard]] constexpr Matrix4D copy() noexcept {
            return {*this};
        }

        /**
         * Adds a translational component to the matrix in the 4th column. The other columns are untouched.
         * @param vector The translation vector to add to the current matrix. (This vector is not modified)
         * @return This matrix for the purpose of chaining methods together.
         */
        constexpr Matrix4D &trn(const PassedVec3 vector) noexcept {
            val[M03] += vector.x;
            val[M13] += vector.y;
            val[M23] += vector.z;
            return *this;
        }

        /**
         * Adds a translational component to the matrix in the 4th column. The other columns are untouched.
         * @param x The x-component of the translation vector.
         * @param y The y-component of the translation vector.
         * @param z The z-component of the translation vector.
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &trn(const T x, const T y, const T z) noexcept {
            val[M03] += x;
            val[M13] += y;
            val[M23] += z;
            return *this;
        }

        [[nodiscard]] constexpr const MatRaw &getValues() const noexcept {
            return val;
        }

        [[nodiscard]] constexpr MatRaw &getValues() noexcept {
            return val;
        }

        /**
         * Postmultiplies this matrix with the given matrix, storing the result in this matrix. For example:
         *
         * <pre>
         * A.mul(B) results in A := AB.
         * </pre>
         * @param matrix The other matrix to multiply by.
         * @return This matrix for the purpose of chaining operations together.
         */
        constexpr Matrix4D &mul(const Matrix4D &matrix) noexcept {
            mul(val, matrix.val);
            return *this;
        }

        /**
         * Premultiplies this matrix with the given matrix, storing the result in this matrix. For example:
         *
         * <pre>
         * A.mulLeft(B) results in A := BA.
         * </pre>
         * @param matrix The other matrix to multiply by.
         * @return This matrix for the purpose of chaining operations together.
         */

        constexpr Matrix4D &mulLeft(const Matrix4D &matrix) noexcept {
            Matrix4D tmp{};
            tmp.set(matrix);
            mul(tmp.val, this->val);
            return set(tmp);
        }

        /**
         * Transposes the matrix.
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &transposes() noexcept {
            std::swap(val[M10], val[M01]);
            std::swap(val[M20], val[M02]);
            std::swap(val[M30], val[M03]);

            std::swap(val[M12], val[M21]);
            std::swap(val[M13], val[M31]);

            std::swap(val[M23], val[M32]);

            return *this;
        }

        /**
         * Sets the matrix to an identity matrix.
         * @return This matrix for the purpose of chaining methods together.
         */
        constexpr Matrix4D &idt() noexcept {
            val[M00] = 1;
            val[M01] = 0;
            val[M02] = 0;
            val[M03] = 0;
            val[M10] = 0;
            val[M11] = 1;
            val[M12] = 0;
            val[M13] = 0;
            val[M20] = 0;
            val[M21] = 0;
            val[M22] = 1;
            val[M23] = 0;
            val[M30] = 0;
            val[M31] = 0;
            val[M32] = 0;
            val[M33] = 1;
            return *this;
        }

        /**
         * Inverts the matrix. Stores the result in this matrix.
         * @return This matrix for the purpose of chaining methods together.
         * @throws RuntimeException if the matrix is singular (not invertible)
         */
        constexpr Matrix4D &inv() {
            const auto l_det = det();

            if(l_det == 0)
                throw std::runtime_error("non-invertible matrix");
            const T inv_det = 1 / l_det;

            MatRaw tmp{};

            tmp[M00] = val[M12] * val[M23] * val[M31] - val[M13] * val[M22] * val[M31] + val[M13] * val[M21] * val[M32] - val[
                    M11]
                * val[M23] * val[M32] - val[M12] * val[M21] * val[M33] + val[M11] * val[M22] * val[M33];
            tmp[M01] = val[M03] * val[M22] * val[M31] - val[M02] * val[M23] * val[M31] - val[M03] * val[M21] * val[M32] + val[
                    M01]
                * val[M23] * val[M32] + val[M02] * val[M21] * val[M33] - val[M01] * val[M22] * val[M33];
            tmp[M02] = val[M02] * val[M13] * val[M31] - val[M03] * val[M12] * val[M31] + val[M03] * val[M11] * val[M32] - val[
                    M01]
                * val[M13] * val[M32] - val[M02] * val[M11] * val[M33] + val[M01] * val[M12] * val[M33];
            tmp[M03] = val[M03] * val[M12] * val[M21] - val[M02] * val[M13] * val[M21] - val[M03] * val[M11] * val[M22] + val[
                    M01]
                * val[M13] * val[M22] + val[M02] * val[M11] * val[M23] - val[M01] * val[M12] * val[M23];
            tmp[M10] = val[M13] * val[M22] * val[M30] - val[M12] * val[M23] * val[M30] - val[M13] * val[M20] * val[M32] + val[
                    M10]
                * val[M23] * val[M32] + val[M12] * val[M20] * val[M33] - val[M10] * val[M22] * val[M33];
            tmp[M11] = val[M02] * val[M23] * val[M30] - val[M03] * val[M22] * val[M30] + val[M03] * val[M20] * val[M32] - val[
                    M00]
                * val[M23] * val[M32] - val[M02] * val[M20] * val[M33] + val[M00] * val[M22] * val[M33];
            tmp[M12] = val[M03] * val[M12] * val[M30] - val[M02] * val[M13] * val[M30] - val[M03] * val[M10] * val[M32] + val[
                    M00]
                * val[M13] * val[M32] + val[M02] * val[M10] * val[M33] - val[M00] * val[M12] * val[M33];
            tmp[M13] = val[M02] * val[M13] * val[M20] - val[M03] * val[M12] * val[M20] + val[M03] * val[M10] * val[M22] - val[
                    M00]
                * val[M13] * val[M22] - val[M02] * val[M10] * val[M23] + val[M00] * val[M12] * val[M23];
            tmp[M20] = val[M11] * val[M23] * val[M30] - val[M13] * val[M21] * val[M30] + val[M13] * val[M20] * val[M31] - val[
                    M10]
                * val[M23] * val[M31] - val[M11] * val[M20] * val[M33] + val[M10] * val[M21] * val[M33];
            tmp[M21] = val[M03] * val[M21] * val[M30] - val[M01] * val[M23] * val[M30] - val[M03] * val[M20] * val[M31] + val[
                    M00]
                * val[M23] * val[M31] + val[M01] * val[M20] * val[M33] - val[M00] * val[M21] * val[M33];
            tmp[M22] = val[M01] * val[M13] * val[M30] - val[M03] * val[M11] * val[M30] + val[M03] * val[M10] * val[M31] - val[
                    M00]
                * val[M13] * val[M31] - val[M01] * val[M10] * val[M33] + val[M00] * val[M11] * val[M33];
            tmp[M23] = val[M03] * val[M11] * val[M20] - val[M01] * val[M13] * val[M20] - val[M03] * val[M10] * val[M21] + val[
                    M00]
                * val[M13] * val[M21] + val[M01] * val[M10] * val[M23] - val[M00] * val[M11] * val[M23];
            tmp[M30] = val[M12] * val[M21] * val[M30] - val[M11] * val[M22] * val[M30] - val[M12] * val[M20] * val[M31] + val[
                    M10]
                * val[M22] * val[M31] + val[M11] * val[M20] * val[M32] - val[M10] * val[M21] * val[M32];
            tmp[M31] = val[M01] * val[M22] * val[M30] - val[M02] * val[M21] * val[M30] + val[M02] * val[M20] * val[M31] - val[
                    M00]
                * val[M22] * val[M31] - val[M01] * val[M20] * val[M32] + val[M00] * val[M21] * val[M32];
            tmp[M32] = val[M02] * val[M11] * val[M30] - val[M01] * val[M12] * val[M30] - val[M02] * val[M10] * val[M31] + val[
                    M00]
                * val[M12] * val[M31] + val[M01] * val[M10] * val[M32] - val[M00] * val[M11] * val[M32];
            tmp[M33] = val[M01] * val[M12] * val[M20] - val[M02] * val[M11] * val[M20] + val[M02] * val[M10] * val[M21] - val[
                    M00]
                * val[M12] * val[M21] - val[M01] * val[M10] * val[M22] + val[M00] * val[M11] * val[M22];

            val[M00] = tmp[M00] * inv_det;
            val[M01] = tmp[M01] * inv_det;
            val[M02] = tmp[M02] * inv_det;
            val[M03] = tmp[M03] * inv_det;
            val[M10] = tmp[M10] * inv_det;
            val[M11] = tmp[M11] * inv_det;
            val[M12] = tmp[M12] * inv_det;
            val[M13] = tmp[M13] * inv_det;
            val[M20] = tmp[M20] * inv_det;
            val[M21] = tmp[M21] * inv_det;
            val[M22] = tmp[M22] * inv_det;
            val[M23] = tmp[M23] * inv_det;
            val[M30] = tmp[M30] * inv_det;
            val[M31] = tmp[M31] * inv_det;
            val[M32] = tmp[M32] * inv_det;
            val[M33] = tmp[M33] * inv_det;
            return *this;
        }

        /** @return The determinant of this matrix */

        [[nodiscard]] constexpr T det() const noexcept {
            return val[M30] * val[M21] * val[M12] * val[M03] - val[M20] * val[M31] * val[M12] * val[M03] - val[M30] * val[M11]
                * val[M22] * val[M03] + val[M10] * val[M31] * val[M22] * val[M03] + val[M20] * val[M11] * val[M32] * val[M03] -
                val[M10]
                * val[M21] * val[M32] * val[M03] - val[M30] * val[M21] * val[M02] * val[M13] + val[M20] * val[M31] * val[M02] *
                val[M13]
                + val[M30] * val[M01] * val[M22] * val[M13] - val[M00] * val[M31] * val[M22] * val[M13] - val[M20] * val[M01] *
                val[M32]
                * val[M13] + val[M00] * val[M21] * val[M32] * val[M13] + val[M30] * val[M11] * val[M02] * val[M23] - val[M10] *
                val[M31]
                * val[M02] * val[M23] - val[M30] * val[M01] * val[M12] * val[M23] + val[M00] * val[M31] * val[M12] * val[M23] +
                val[M10]
                * val[M01] * val[M32] * val[M23] - val[M00] * val[M11] * val[M32] * val[M23] - val[M20] * val[M11] * val[M02] *
                val[M33]
                + val[M10] * val[M21] * val[M02] * val[M33] + val[M20] * val[M01] * val[M12] * val[M33] - val[M00] * val[M21] *
                val[M12]
                * val[M33] - val[M10] * val[M01] * val[M22] * val[M33] + val[M00] * val[M11] * val[M22] * val[M33];
        }

        /** @return The determinant of the 3x3 upper left matrix */

        [[nodiscard]] constexpr T det3x3() const noexcept {
            return val[M00] * val[M11] * val[M22] + val[M01] * val[M12] * val[M20] + val[M02] * val[M10] * val[M21] - val[M00]
                * val[M12] * val[M21] - val[M01] * val[M10] * val[M22] - val[M02] * val[M11] * val[M20];
        }

        /**
         * Sets the matrix to a projection matrix with a near- and far plane, a field of view in degrees and an aspect ratio. Note that
         * the field of view specified is the angle in degrees for the height, the field of view for the width will be calculated
         * according to the aspect ratio.
         * @param near The near plane
         * @param far The far plane
         * @param fovy The field of view of the height in degrees
         * @param aspectRatio The "width over height" aspect ratio
         * @return This matrix for the purpose of chaining methods together.
         */

        Matrix4D &setToProjection(const T near, const T far, const T fovy, const T aspectRatio) {
            idt();
            const T l_fd = static_cast<T>(1.0 / std::tan((fovy * (Math::PI / 180)) / 2.0));
            const T l_a1 = (far + near) / (near - far);
            const T l_a2 = (2 * far * near) / (near - far);

            val[M00] = l_fd / aspectRatio;
            val[M10] = 0;
            val[M20] = 0;
            val[M30] = 0;
            val[M01] = 0;
            val[M11] = l_fd;
            val[M21] = 0;
            val[M31] = 0;
            val[M02] = 0;
            val[M12] = 0;
            val[M22] = l_a1;
            val[M32] = -1;
            val[M03] = 0;
            val[M13] = 0;
            val[M23] = l_a2;
            val[M33] = 0;

            return *this;
        }

        /**
         * Sets the matrix to a projection matrix with a near/far plane, and left, bottom, right and top specifying the points on the
         * near plane that are mapped to the lower left and upper right corners of the viewport. This allows to create projection
         * matrix with off-center vanishing point.
         * @param near The near plane
         * @param far The far plane
         * @return This matrix for the purpose of chaining methods together.
         */
        constexpr Matrix4D &setToProjection(const T left, const T right, const T bottom, const T top, const T near,
                                         const T far) noexcept {
            const T x = 2.0f * near / (right - left);
            const T y = 2.0f * near / (top - bottom);
            const T a = (right + left) / (right - left);
            const T b = (top + bottom) / (top - bottom);
            const T l_a1 = (far + near) / (near - far);
            const T l_a2 = (2 * far * near) / (near - far);
            val[M00] = x;
            val[M10] = 0;
            val[M20] = 0;
            val[M30] = 0;
            val[M01] = 0;
            val[M11] = y;
            val[M21] = 0;
            val[M31] = 0;
            val[M02] = a;
            val[M12] = b;
            val[M22] = l_a1;
            val[M32] = -1;
            val[M03] = 0;
            val[M13] = 0;
            val[M23] = l_a2;
            val[M33] = 0;

            return *this;
        }

        /**
         * Sets this matrix to an orthographic projection matrix with the origin at (x,y) extending by width and height. The near plane
         * is set to 0, the far plane is set to 1.
         * @param x The x-coordinate of the origin
         * @param y The y-coordinate of the origin
         * @param width The width
         * @param height The height
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &setToOrtho2D(const T x, const T y, const T width, const T height) noexcept {
            setToOrtho(x, x + width, y, y + height, 0, 1);
            return *this;
        }

        /**
         * Sets this matrix to an orthographic projection matrix with the origin at (x,y) extending by width and height, having a near
         * and far plane.
         * @param x The x-coordinate of the origin
         * @param y The y-coordinate of the origin
         * @param width The width
         * @param height The height
         * @param near The near plane
         * @param far The far plane
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &setToOrtho2D(const T x, const T y, const T width, const T height, const T near, const T far) noexcept {
            setToOrtho(x, x + width, y, y + height, near, far);
            return *this;
        }

        /**
         * Sets the matrix to an orthographic projection like glOrtho (http://www.opengl.org/sdk/docs/man/xhtml/glOrtho.xml) following
         * the OpenGL equivalent
         * @param left The left clipping plane
         * @param right The right clipping plane
         * @param bottom The bottom clipping plane
         * @param top The top clipping plane
         * @param near The near clipping plane
         * @param far The far clipping plane
         * @return This matrix for the purpose of chaining methods together.
         */
        constexpr Matrix4D &setToOrtho(const T left, const T right, const T bottom, const T top, const T near,
                                    const T far) noexcept {

            this->idt();
            const T x_orth = 2 / (right - left);
            const T y_orth = 2 / (top - bottom);
            const T z_orth = -2 / (far - near);

            const T tx = -(right + left) / (right - left);
            const T ty = -(top + bottom) / (top - bottom);
            const T tz = -(far + near) / (far - near);

            val[M00] = x_orth;
            val[M10] = 0;
            val[M20] = 0;
            val[M30] = 0;
            val[M01] = 0;
            val[M11] = y_orth;
            val[M21] = 0;
            val[M31] = 0;
            val[M02] = 0;
            val[M12] = 0;
            val[M22] = z_orth;
            val[M32] = 0;
            val[M03] = tx;
            val[M13] = ty;
            val[M23] = tz;
            val[M33] = 1;

            return *this;
        }

        /**
         * Sets the 4th column to the translation vector.
         * @param vector The translation vector
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &setTranslation(const PassedVec3 vector) noexcept {
            val[M03] = vector.x;
            val[M13] = vector.y;
            val[M23] = vector.z;
            return *this;
        }

        /**
         * Sets the 4th column to the translation vector.
         * @param x The X coordinate of the translation vector
         * @param y The Y coordinate of the translation vector
         * @param z The Z coordinate of the translation vector
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &setTranslation(const T x, const T y, const T z) noexcept {
            val[M03] = x;
            val[M13] = y;
            val[M23] = z;
            return *this;
        }

        /**
         * Sets this matrix to a translation matrix, overwriting it first by an identity matrix and then setting the 4th column to the
         * translation vector.
         * @param vector The translation vector
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &setToTranslation(const PassedVec3 vector) noexcept {
            idt();
            val[M03] = vector.x;
            val[M13] = vector.y;
            val[M23] = vector.z;
            return *this;
        }

        /**
         * Sets this matrix to a translation matrix, overwriting it first by an identity matrix and then setting the 4th column to the
         * translation vector.
         * @param x The x-component of the translation vector.
         * @param y The y-component of the translation vector.
         * @param z The z-component of the translation vector.
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &setToTranslation(const T x, const T y, const T z) noexcept {
            idt();
            val[M03] = x;
            val[M13] = y;
            val[M23] = z;
            return *this;
        }

        /**
         * Sets this matrix to a translation and scaling matrix by first overwriting it with an identity and then setting the
         * translation vector in the 4th column and the scaling vector in the diagonal.
         * @param translation The translation vector
         * @param scaling The scaling vector
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &setToTranslationAndScaling(Vec3 translation, Vec3 scaling) noexcept {
            idt();
            val[M03] = translation.x;
            val[M13] = translation.y;
            val[M23] = translation.z;
            val[M00] = scaling.x;
            val[M11] = scaling.y;
            val[M22] = scaling.z;
            return *this;
        }

        /**
         * Sets this matrix to a translation and scaling matrix by first overwriting it with an identity and then setting the
         * translation vector in the 4th column and the scaling vector in the diagonal.
         * @param translationX The x-component of the translation vector
         * @param translationY The y-component of the translation vector
         * @param translationZ The z-component of the translation vector
         * @param scalingX The x-component of the scaling vector
         * @param scalingY The x-component of the scaling vector
         * @param scalingZ The x-component of the scaling vector
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &setToTranslationAndScaling(const T translationX, const T translationY, const T translationZ,
                                                    const T scalingX,
                                                    const T scalingY, const T scalingZ) noexcept {
            idt();
            val[M03] = translationX;
            val[M13] = translationY;
            val[M23] = translationZ;
            val[M00] = scalingX;
            val[M11] = scalingY;
            val[M22] = scalingZ;
            return *this;
        }

        /**
         * Sets this matrix to a scaling matrix
         * @param vector The scaling vector
         * @return This matrix for chaining.
         */
        constexpr Matrix4D &setToScaling(const PassedVec3 vector) noexcept {
            idt();
            val[M00] = vector.x;
            val[M11] = vector.y;
            val[M22] = vector.z;
            return *this;
        }

        /**
         * Sets this matrix to a scaling matrix
         * @param x The x-component of the scaling vector
         * @param y The y-component of the scaling vector
         * @param z The z-component of the scaling vector
         * @return This matrix for chaining.
         */

        constexpr Matrix4D &setToScaling(const T x, const T y, const T z) noexcept {
            idt();
            val[M00] = x;
            val[M11] = y;
            val[M22] = z;
            return *this;
        }

        /**
         * Sets the matrix to a look at matrix with a direction and an up vector. Multiply with a translation matrix to get a camera
         * model view matrix.
         * @param direction The direction vector
         * @param up The up vector
         * @return This matrix for the purpose of chaining methods together.
         */

        Matrix4D &setToLookAt(const PassedVec3 direction, const PassedVec3 up) noexcept {
            Vector3D<T> l_vez{};
            Vector3D<T> l_vex{};
            Vector3D<T> l_vey{};

            l_vez.set(direction).normalize();
            l_vex.set(direction).normalize();
            l_vex.cross(up).normalize();
            l_vey.set(l_vex).cross(l_vez).normalize();
            idt();
            val[M00] = l_vex.x;
            val[M01] = l_vex.y;
            val[M02] = l_vex.z;
            val[M10] = l_vey.x;
            val[M11] = l_vey.y;
            val[M12] = l_vey.z;
            val[M20] = -l_vez.x;
            val[M21] = -l_vez.y;
            val[M22] = -l_vez.z;

            return *this;
        }

        /**
         * Sets this matrix to a look at matrix with the given position, target and up vector.
         * @param position the position
         * @param target the target
         * @param up the up vector
         * @return This matrix
         */
        Matrix4D &setToLookAt(const PassedVec3 position, const PassedVec3 target, const PassedVec3 up) noexcept {
            Vector3D<T> tmpVec{};
            Matrix4D tmpMat{};

            tmpVec.set(target).sub(position);
            setToLookAt(tmpVec, up);
            this->mul(tmpMat.setToTranslation(-position.x, -position.y, -position.z));

            return *this;
        }


        Matrix4D &setToWorld(const PassedVec3 position, const PassedVec3 forward, const PassedVec3 up) noexcept {
            Vector3D<T> right{};
            Vector3D<T> tmpForward{};
            Vector3D<T> tmpUp{};

            tmpForward.set(forward).normalize();
            right.set(tmpForward).cross(up).normalize();
            tmpUp.set(right).cross(tmpForward).normalize();

            this->set(right, tmpUp, tmpForward.scl(-1), position);
            return *this;
        }


        friend std::ostream &operator<<(std::ostream &os, const Matrix4D &obj) {
            return os << "[" << obj.val[M00] << "|" << obj.val[M01] << "|" << obj.val[M02] << "|" << obj.val[M03] << "]\n" <<
                "[" << obj.val[M10] << "|" << obj.val[M11] <<
                "|"
                << obj.val[M12] << "|" << obj.val[M13] << "]\n" << "[" << obj.val[M20] << "|" << obj.val[M21] << "|" << obj.val[
                    M22] << "|" << obj.val[M23] << "]\n"
                << "["
                << obj.val[M30] << "|" << obj.val[M31] << "|" << obj.val[M32] << "|" << obj.val[M33] << "]\n";
        }

        /**
         * Linearly interpolates between this matrix and the given matrix mixing by alpha
         * @param matrix the matrix
         * @param alpha the alpha value in the range [0,1]
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &lerp(const Matrix4D &matrix, const T alpha) noexcept {
            for(int i = 0; i < 16; i++)
                this->val[i] = this->val[i] * (1 - alpha) + matrix.val[i] * alpha;
            return *this;
        }

        /**
         * Assumes that both matrices are 2D affine transformations, copying only the relevant components. The copied values are:
         *
         * <pre>
         *      [  M00  M01   _   M03  ]
         *      [  M10  M11   _   M13  ]
         *      [   _    _    _    _   ]
         *      [   _    _    _    _   ]
         * </pre>
         * @param mat the source matrix
         * @return This matrix for chaining
         */
        constexpr Matrix4D &setAsAffine(const Matrix4D mat) noexcept {
            val[M00] = mat.val[M00];
            val[M10] = mat.val[M10];
            val[M01] = mat.val[M01];
            val[M11] = mat.val[M11];
            val[M03] = mat.val[M03];
            val[M13] = mat.val[M13];
            return *this;
        }


        constexpr Matrix4D &scl(const PassedVec3 scale) noexcept {
            val[M00] *= scale.x;
            val[M11] *= scale.y;
            val[M22] *= scale.z;
            return *this;
        }


        constexpr Matrix4D &scl(const T x, const T y, const T z) noexcept {
            val[M00] *= x;
            val[M11] *= y;
            val[M22] *= z;
            return *this;
        }


        constexpr Matrix4D &scl(const T scale) noexcept {
            val[M00] *= scale;
            val[M11] *= scale;
            val[M22] *= scale;
            return *this;
        }


        [[nodiscard]] constexpr Vector3D<T> getTranslation() const noexcept {
            Vector3D<T> position{};

            position.x = val[M03];
            position.y = val[M13];
            position.z = val[M23];
            return position;
        }


        /** @return the squared scale factor on the X axis */
        [[nodiscard]] T getScaleXSquared() const {
            return val[M00] * val[M00] + val[M01] * val[M01] + val[M02] * val[M02];
        }

        /** @return the squared scale factor on the Y axis */

        [[nodiscard]] T getScaleYSquared() const {
            return val[M10] * val[M10] + val[M11] * val[M11] + val[M12] * val[M12];
        }

        /** @return the squared scale factor on the Z axis */

        [[nodiscard]] T getScaleZSquared() const {
            return val[M20] * val[M20] + val[M21] * val[M21] + val[M22] * val[M22];
        }

        /** @return the scale factor on the X axis (non-negative) */

        [[nodiscard]] T getScaleX() const {
            return (Math::zero(val[M01]) && Math::zero(val[M02]))
                ? Math::abs(val[M00])
                : Math::sqrt(getScaleXSquared());
        }

        /** @return the scale factor on the Y axis (non-negative) */

        [[nodiscard]] T getScaleY() const {
            return (Math::zero(val[M10]) && Math::zero(val[M12]))
                ? Math::abs(val[M11])
                : Math::sqrt(getScaleYSquared());
        }

        /** @return the scale factor on the X axis (non-negative) */

        [[nodiscard]] T getScaleZ() const {
            return (Math::zero(val[M20]) && Math::zero(val[M21]))
                ? Math::abs(val[M22])
                : Math::sqrt(getScaleZSquared());
        }

        /**
         * @return The provided vector for chaining.
         */
        [[nodiscard]] auto getScale() const {
            Vector3D<T> scale{};
            return scale.set(getScaleX(), getScaleY(), getScaleZ());
        }

        /** removes the translational part and transposes the matrix. */

        constexpr Matrix4D &toNormalMatrix() noexcept {
            val[M03] = 0;
            val[M13] = 0;
            val[M23] = 0;
            return inv().transposes();
        }

        /**
         * Multiplies the matrix mata with matrix matb, storing the result in mata. The arrays are assumed to hold 4x4 column major
         * matrices as you can get from @link Matrix4D#val @endlink. This is the same as @link Matrix4D#mul @endlink.
         * @param mata the first matrix.
         * @param matb the second matrix.
         */

        constexpr static void mul(MatRaw &mata, const MatRaw &matb) {
            MatRaw tmp{};
            tmp[M00] = mata[M00] * matb[M00] + mata[M01] * matb[M10] + mata[M02] * matb[M20] + mata[M03] * matb[M30];
            tmp[M01] = mata[M00] * matb[M01] + mata[M01] * matb[M11] + mata[M02] * matb[M21] + mata[M03] * matb[M31];
            tmp[M02] = mata[M00] * matb[M02] + mata[M01] * matb[M12] + mata[M02] * matb[M22] + mata[M03] * matb[M32];
            tmp[M03] = mata[M00] * matb[M03] + mata[M01] * matb[M13] + mata[M02] * matb[M23] + mata[M03] * matb[M33];
            tmp[M10] = mata[M10] * matb[M00] + mata[M11] * matb[M10] + mata[M12] * matb[M20] + mata[M13] * matb[M30];
            tmp[M11] = mata[M10] * matb[M01] + mata[M11] * matb[M11] + mata[M12] * matb[M21] + mata[M13] * matb[M31];
            tmp[M12] = mata[M10] * matb[M02] + mata[M11] * matb[M12] + mata[M12] * matb[M22] + mata[M13] * matb[M32];
            tmp[M13] = mata[M10] * matb[M03] + mata[M11] * matb[M13] + mata[M12] * matb[M23] + mata[M13] * matb[M33];
            tmp[M20] = mata[M20] * matb[M00] + mata[M21] * matb[M10] + mata[M22] * matb[M20] + mata[M23] * matb[M30];
            tmp[M21] = mata[M20] * matb[M01] + mata[M21] * matb[M11] + mata[M22] * matb[M21] + mata[M23] * matb[M31];
            tmp[M22] = mata[M20] * matb[M02] + mata[M21] * matb[M12] + mata[M22] * matb[M22] + mata[M23] * matb[M32];
            tmp[M23] = mata[M20] * matb[M03] + mata[M21] * matb[M13] + mata[M22] * matb[M23] + mata[M23] * matb[M33];
            tmp[M30] = mata[M30] * matb[M00] + mata[M31] * matb[M10] + mata[M32] * matb[M20] + mata[M33] * matb[M30];
            tmp[M31] = mata[M30] * matb[M01] + mata[M31] * matb[M11] + mata[M32] * matb[M21] + mata[M33] * matb[M31];
            tmp[M32] = mata[M30] * matb[M02] + mata[M31] * matb[M12] + mata[M32] * matb[M22] + mata[M33] * matb[M32];
            tmp[M33] = mata[M30] * matb[M03] + mata[M31] * matb[M13] + mata[M32] * matb[M23] + mata[M33] * matb[M33];
            mata = tmp;
        }


        /**
         * Postmultiplies this matrix by a translation matrix. Postmultiplication is also used by OpenGL ES'
         * glTranslate/glRotate/glScale
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &translate(const PassedVec3 translation) noexcept {
            return translate(translation.x, translation.y, translation.z);
        }

        /**
         * Postmultiplies this matrix by a translation matrix. Postmultiplication is also used by OpenGL ES' 1.x
         * glTranslate/glRotate/glScale.
         * @param x Translation in the x-axis.
         * @param y Translation in the y-axis.
         * @param z Translation in the z-axis.
         * @return This matrix for the purpose of chaining methods together.
         */
        constexpr Matrix4D &translate(const T x, const T y, const T z) noexcept {
            MatRaw tmp2{};
            tmp2[M00] = 1;
            tmp2[M01] = 0;
            tmp2[M02] = 0;
            tmp2[M03] = x;
            tmp2[M10] = 0;
            tmp2[M11] = 1;
            tmp2[M12] = 0;
            tmp2[M13] = y;
            tmp2[M20] = 0;
            tmp2[M21] = 0;
            tmp2[M22] = 1;
            tmp2[M23] = z;
            tmp2[M30] = 0;
            tmp2[M31] = 0;
            tmp2[M32] = 0;
            tmp2[M33] = 1;

            mul(val, tmp2);
            return *this;
        }

        /**
         * Postmultiplies this matrix with a (counter-clockwise) rotation matrix. Postmultiplication is also used by OpenGL ES' 1.x
         * glTranslate/glRotate/glScale.
         * @param axis The vector axis to rotate around.
         * @param degrees The angle in degrees.
         * @return This matrix for the purpose of chaining methods together.
         */
        Matrix4D& rotate(Vec3 axis, T degrees) {
            if(degrees == 0)
                return *this;

            PassedQuat quat{};
            quat.set(axis, degrees);
            return rotate(quat);
        }

        /**
         * Postmultiplies this matrix with a (counter-clockwise) rotation matrix. Postmultiplication is also used by OpenGL ES' 1.x
         * glTranslate/glRotate/glScale.
         * @param axis The vector axis to rotate around.
         * @param radians The angle in radians.
         * @return This matrix for the purpose of chaining methods together.
         */
        Matrix4D& rotateRad(Vec3 axis, T radians) {
            if(radians == 0)
                return *this;

            PassedQuat quat{};
            quat.setFromAxisRad(axis, radians);
            return rotate(quat);
        }

        /**
         * Postmultiplies this matrix with a (counter-clockwise) rotation matrix. Postmultiplication is also used by OpenGL ES' 1.x
         * glTranslate/glRotate/glScale
         * @param axisX The x-axis component of the vector to rotate around.
         * @param axisY The y-axis component of the vector to rotate around.
         * @param axisZ The z-axis component of the vector to rotate around.
         * @param degrees The angle in degrees
         * @return This matrix for the purpose of chaining methods together.
         */
        Matrix4D& rotate(T axisX, T axisY, T axisZ, T degrees) {
            if(degrees == 0)
                return *this;

            PassedQuat quat{};
            quat.setFromAxis(axisX, axisY, axisZ, degrees);
            return rotate(quat);
        }

        /**
         * Postmultiplies this matrix with a (counter-clockwise) rotation matrix. Postmultiplication is also used by OpenGL ES' 1.x
         * glTranslate/glRotate/glScale
         * @param axisX The x-axis component of the vector to rotate around.
         * @param axisY The y-axis component of the vector to rotate around.
         * @param axisZ The z-axis component of the vector to rotate around.
         * @param radians The angle in radians
         * @return This matrix for the purpose of chaining methods together.
         */
        Matrix4D& rotateRad(T axisX, T axisY, T axisZ, T radians) {
            if(radians == 0)
                return *this;

            PassedQuat quat{};
            quat.setFromAxisRad(axisX, axisY, axisZ, radians);
            return rotate(quat);
        }

        Matrix4D& rotate(const PassedQuat& rotation){
            MatRaw tmp = quatToMatrix(rotation);
            mul(val, tmp);
            return *this;
        }

        /**
         * Postmultiplies this matrix by the rotation between two vectors.
         * @param v1 The base vector
         * @param v2 The target vector
         * @return This matrix for the purpose of chaining methods together
         */
        Matrix4D& rotate(const PassedVec3 v1, const PassedVec3 v2) {
            PassedQuat quat{};
            return rotate(quat.setFromCross(v1, v2));
        }


        constexpr Matrix4D &scale(PassedVec3 vec) {
            return scale(vec.x, vec.y, vec.z);
        }

        /**
         * Postmultiplies this matrix with a scale matrix. Postmultiplication is also used by OpenGL ES' 1.x
         * glTranslate/glRotate/glScale.
         * @param scaleX The scale in the x-axis.
         * @param scaleY The scale in the y-axis.
         * @param scaleZ The scale in the z-axis.
         * @return This matrix for the purpose of chaining methods together.
         */

        constexpr Matrix4D &scale(const T scaleX, const T scaleY, const T scaleZ) {
            MatRaw tmp2{};
            tmp2[M00] = scaleX;
            tmp2[M01] = 0;
            tmp2[M02] = 0;
            tmp2[M03] = 0;
            tmp2[M10] = 0;
            tmp2[M11] = scaleY;
            tmp2[M12] = 0;
            tmp2[M13] = 0;
            tmp2[M20] = 0;
            tmp2[M21] = 0;
            tmp2[M22] = scaleZ;
            tmp2[M23] = 0;
            tmp2[M30] = 0;
            tmp2[M31] = 0;
            tmp2[M32] = 0;
            tmp2[M33] = 1;

            mul(val, tmp2);
            return *this;
        }


        /** @return True if this matrix has any rotation or scaling, false otherwise */

        [[nodiscard]] bool hasRotationOrScaling() const {
            return !(Math::equal(val[M00], 1) && Math::equal(val[M11], 1) && Math::equal(val[M22], 1)
                && Math::zero(val[M01]) && Math::zero(val[M02]) && Math::zero(val[M10]) && Math::zero(val[M12])
                && Math::zero(val[M20]) && Math::zero(val[M21]));
        }


        static constexpr MatRaw quatToMatrix (const PassedQuat& quat) noexcept {
            MatRaw matrix{};
            const T xx = quat.x * quat.x;
            const T xy = quat.x * quat.y;
            const T xz = quat.x * quat.z;
            const T xw = quat.x * quat.w;
            const T yy = quat.y * quat.y;
            const T yz = quat.y * quat.z;
            const T yw = quat.y * quat.w;
            const T zz = quat.z * quat.z;
            const T zw = quat.z * quat.w;
            // Set matrix from quaternion
            matrix[M00] = 1 - 2 * (yy + zz);
            matrix[M01] = 2 * (xy - zw);
            matrix[M02] = 2 * (xz + yw);
            matrix[M03] = 0;
            matrix[M10] = 2 * (xy + zw);
            matrix[M11] = 1 - 2 * (xx + zz);
            matrix[M12] = 2 * (yz - xw);
            matrix[M13] = 0;
            matrix[M20] = 2 * (xz - yw);
            matrix[M21] = 2 * (yz + xw);
            matrix[M22] = 1 - 2 * (xx + yy);
            matrix[M23] = 0;
            matrix[M30] = 0;
            matrix[M31] = 0;
            matrix[M32] = 0;
            matrix[M33] = 1;

            return matrix;
        }

    };

    using Mat4 = Matrix4D;
}

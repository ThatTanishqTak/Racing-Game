#pragma once

#include "Powertrain/Math/Quaternion.hpp"
#include "Powertrain/Math/Vector3.hpp"
#include "Powertrain/Math/Vector4.hpp"

namespace Powertrain
{
	struct Matrix4
	{
		float M[4][4] =
		{
			{ 1.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 1.0f }
		};

		static constexpr Matrix4 Identity() { return {}; }

		static constexpr Matrix4 Translation(const Vector3& translation)
		{
			Matrix4 l_Result;
			l_Result.M[3][0] = translation.X;
			l_Result.M[3][1] = translation.Y;
			l_Result.M[3][2] = translation.Z;

			return l_Result;
		}

		static constexpr Matrix4 Scaling(const Vector3& scale)
		{
			Matrix4 l_Result;
			l_Result.M[0][0] = scale.X;
			l_Result.M[1][1] = scale.Y;
			l_Result.M[2][2] = scale.Z;

			return l_Result;
		}

		static Matrix4 RotationX(float radians);
		static Matrix4 RotationY(float radians);
		static Matrix4 RotationZ(float radians);
		static Matrix4 FromQuaternion(const Quaternion& rotation);
		static Matrix4 TRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale);
		static Matrix4 LookAt(const Vector3& eye, const Vector3& target, const Vector3& up);
		static Matrix4 Perspective(float verticalFovRadians, float aspectRatio, float nearPlane);
		static Matrix4 Orthographic(float width, float height, float nearPlane, float farPlane);

		Matrix4 Transposed() const;

		// A singular matrix returns the identity
		Matrix4 Inverse() const;
		float Determinant() const;

		// Fails on a singular or skewed matrix
		bool Decompose(Vector3& scale, Quaternion& rotation, Vector3& translation) const;

		// Point: W = 1, no perspective divide. Direction: W = 0, ignores the translation
		constexpr Vector3 TransformPoint(const Vector3& point) const
		{
			return
			{
				point.X * M[0][0] + point.Y * M[1][0] + point.Z * M[2][0] + M[3][0],
				point.X * M[0][1] + point.Y * M[1][1] + point.Z * M[2][1] + M[3][1],
				point.X * M[0][2] + point.Y * M[1][2] + point.Z * M[2][2] + M[3][2]
			};
		}

		constexpr Vector3 TransformDirection(const Vector3& direction) const
		{
			return
			{
				direction.X * M[0][0] + direction.Y * M[1][0] + direction.Z * M[2][0],
				direction.X * M[0][1] + direction.Y * M[1][1] + direction.Z * M[2][1],
				direction.X * M[0][2] + direction.Y * M[1][2] + direction.Z * M[2][2]
			};
		}

		constexpr Vector4 TransformVector(const Vector4& vector) const
		{
			return
			{
				vector.X * M[0][0] + vector.Y * M[1][0] + vector.Z * M[2][0] + vector.W * M[3][0],
				vector.X * M[0][1] + vector.Y * M[1][1] + vector.Z * M[2][1] + vector.W * M[3][1],
				vector.X * M[0][2] + vector.Y * M[1][2] + vector.Z * M[2][2] + vector.W * M[3][2],
				vector.X * M[0][3] + vector.Y * M[1][3] + vector.Z * M[2][3] + vector.W * M[3][3]
			};
		}

		constexpr Vector3 GetTranslation() const { return { M[3][0], M[3][1], M[3][2] }; }
		constexpr void SetTranslation(const Vector3& translation) { M[3][0] = translation.X; M[3][1] = translation.Y; M[3][2] = translation.Z; }

		// Basis axes: row 0 is X, row 1 is Y, row 2 is Z
		constexpr Vector3 GetAxis(int row) const { return { M[row][0], M[row][1], M[row][2] }; }

		Matrix4& operator*=(const Matrix4& other);

		constexpr bool operator==(const Matrix4&) const = default;
	};

	// a first, then b
	Matrix4 operator*(const Matrix4& a, const Matrix4& b);

	// Row-vector transform, v * M
	constexpr Vector4 operator*(const Vector4& v, const Matrix4& m) { return m.TransformVector(v); }

	static_assert(sizeof(Matrix4) == 16 * sizeof(float), "Matrix4 must be 16 contiguous floats");
}
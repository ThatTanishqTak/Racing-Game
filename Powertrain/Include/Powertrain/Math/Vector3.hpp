#pragma once

#include "Powertrain/Math/MathUtilities.hpp"

#include <cmath>

namespace Powertrain
{
	// Right-handed, +Y up (confirmed at M5; matches Blender, glTF and the M4 debug scene)
	struct Vector3
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;

		static constexpr Vector3 Zero() { return { 0.0f, 0.0f, 0.0f }; }
		static constexpr Vector3 One() { return { 1.0f, 1.0f, 1.0f }; }
		static constexpr Vector3 UnitX() { return { 1.0f, 0.0f, 0.0f }; }
		static constexpr Vector3 UnitY() { return { 0.0f, 1.0f, 0.0f }; }
		static constexpr Vector3 UnitZ() { return { 0.0f, 0.0f, 1.0f }; }
		static constexpr Vector3 Up() { return UnitY(); }

		static constexpr float Dot(const Vector3& a, const Vector3& b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }
		static constexpr Vector3 Cross(const Vector3& a, const Vector3& b) { return { a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X }; }
		static constexpr Vector3 Lerp(const Vector3& a, const Vector3& b, float t) { return { Math::Lerp(a.X, b.X, t), Math::Lerp(a.Y, b.Y, t), Math::Lerp(a.Z, b.Z, t) }; }
		static constexpr Vector3 Min(const Vector3& a, const Vector3& b) { return { a.X < b.X ? a.X : b.X, a.Y < b.Y ? a.Y : b.Y, a.Z < b.Z ? a.Z : b.Z }; }
		static constexpr Vector3 Max(const Vector3& a, const Vector3& b) { return { a.X > b.X ? a.X : b.X, a.Y > b.Y ? a.Y : b.Y, a.Z > b.Z ? a.Z : b.Z }; }
		static float Distance(const Vector3& a, const Vector3& b) { return (b - a).Length(); }

		constexpr float LengthSquared() const { return X * X + Y * Y + Z * Z; }
		float Length() const { return std::sqrt(LengthSquared()); }

		// Zero stays zero instead of producing NaN
		Vector3 Normalized() const
		{
			const float l_Length = Length();

			return l_Length > Math::k_Epsilon ? Vector3{ X / l_Length, Y / l_Length, Z / l_Length } : Zero();
		}

		// Any unit vector at right angles to this unit direction; crosses with the axis it is least aligned with
		Vector3 Perpendicular() const
		{
			const float l_AbsX = Math::Abs(X);
			const float l_AbsY = Math::Abs(Y);
			const float l_AbsZ = Math::Abs(Z);

			Vector3 l_Axis = UnitX();
			if (l_AbsY <= l_AbsX && l_AbsY <= l_AbsZ)
			{
				l_Axis = UnitY();
			}
			else if (l_AbsZ <= l_AbsX && l_AbsZ <= l_AbsY)
			{
				l_Axis = UnitZ();
			}

			return Cross(*this, l_Axis).Normalized();
		}

		constexpr Vector3 operator-() const { return { -X, -Y, -Z }; }
		constexpr Vector3& operator+=(const Vector3& other) { X += other.X; Y += other.Y; Z += other.Z; return *this; }
		constexpr Vector3& operator-=(const Vector3& other) { X -= other.X; Y -= other.Y; Z -= other.Z; return *this; }
		constexpr Vector3& operator*=(float scalar) { X *= scalar; Y *= scalar; Z *= scalar; return *this; }
		constexpr Vector3& operator/=(float scalar) { X /= scalar; Y /= scalar; Z /= scalar; return *this; }

		constexpr bool operator==(const Vector3&) const = default;

		friend constexpr Vector3 operator+(const Vector3& a, const Vector3& b) { return { a.X + b.X, a.Y + b.Y, a.Z + b.Z }; }
		friend constexpr Vector3 operator-(const Vector3& a, const Vector3& b) { return { a.X - b.X, a.Y - b.Y, a.Z - b.Z }; }
		friend constexpr Vector3 operator*(const Vector3& a, const Vector3& b) { return { a.X * b.X, a.Y * b.Y, a.Z * b.Z }; }
		friend constexpr Vector3 operator*(const Vector3& v, float scalar) { return { v.X * scalar, v.Y * scalar, v.Z * scalar }; }
		friend constexpr Vector3 operator*(float scalar, const Vector3& v) { return v * scalar; }
		friend constexpr Vector3 operator/(const Vector3& v, float scalar) { return { v.X / scalar, v.Y / scalar, v.Z / scalar }; }
	};
}
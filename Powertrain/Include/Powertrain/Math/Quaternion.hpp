#pragma once

#include "Powertrain/Math/MathUtilities.hpp"
#include "Powertrain/Math/Vector3.hpp"

#include <cmath>

namespace Powertrain
{
	struct Quaternion
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;
		float W = 1.0f;

		static constexpr Quaternion Identity() { return {}; }

		// Axis need not be unit length; a zero axis gives the identity
		static Quaternion FromAxisAngle(const Vector3& axis, float radians);

		// Radians. Applied as roll about Z, then pitch about X, then yaw about Y
		static Quaternion FromEulerAngles(float pitch, float yaw, float roll);
		static Quaternion FromEulerAngles(const Vector3& pitchYawRoll) { return FromEulerAngles(pitchYawRoll.X, pitchYawRoll.Y, pitchYawRoll.Z); }

		// Shortest-arc spherical interpolation, t in [0, 1]
		static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);

		static constexpr float Dot(const Quaternion& a, const Quaternion& b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W; }

		constexpr float LengthSquared() const { return X * X + Y * Y + Z * Z + W * W; }
		float Length() const { return std::sqrt(LengthSquared()); }

		// Zero length returns the identity instead of NaN
		Quaternion Normalized() const
		{
			const float l_Length = Length();

			return l_Length > Math::k_Epsilon ? Quaternion{ X / l_Length, Y / l_Length, Z / l_Length, W / l_Length } : Identity();
		}

		// The inverse of a unit quaternion
		constexpr Quaternion Conjugate() const { return { -X, -Y, -Z, W }; }

		// Full inverse, valid for non-unit quaternions too
		Quaternion Inverse() const;

		Vector3 Rotate(const Vector3& vector) const;

		// Axis is unit length, or zero for the identity; the angle is in [0, 2 pi]
		void ToAxisAngle(Vector3& axis, float& radians) const;

		constexpr bool operator==(const Quaternion&) const = default;
	};

	// Rotation a followed by rotation b
	Quaternion operator*(const Quaternion& a, const Quaternion& b);
}
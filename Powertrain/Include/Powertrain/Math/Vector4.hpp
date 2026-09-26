#pragma once

#include "Powertrain/Math/MathUtilities.hpp"
#include "Powertrain/Math/Vector3.hpp"

#include <cmath>

namespace Powertrain
{
	struct Vector4
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;
		float W = 0.0f;

		static constexpr Vector4 Zero() { return { 0.0f, 0.0f, 0.0f, 0.0f }; }
		static constexpr Vector4 One() { return { 1.0f, 1.0f, 1.0f, 1.0f }; }

		// W = 1 for points, 0 for directions
		static constexpr Vector4 FromVector3(const Vector3& v, float w) { return { v.X, v.Y, v.Z, w }; }
		static constexpr Vector4 Point(const Vector3& v) { return FromVector3(v, 1.0f); }
		static constexpr Vector4 Direction(const Vector3& v) { return FromVector3(v, 0.0f); }

		static constexpr float Dot(const Vector4& a, const Vector4& b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W; }
		static constexpr Vector4 Lerp(const Vector4& a, const Vector4& b, float t) { return { Math::Lerp(a.X, b.X, t), Math::Lerp(a.Y, b.Y, t), Math::Lerp(a.Z, b.Z, t), Math::Lerp(a.W, b.W, t) }; }

		constexpr Vector3 XYZ() const { return { X, Y, Z }; }

		// Perspective divide; W must be non-zero
		constexpr Vector3 Homogenized() const { return { X / W, Y / W, Z / W }; }

		constexpr float LengthSquared() const { return X * X + Y * Y + Z * Z + W * W; }
		float Length() const { return std::sqrt(LengthSquared()); }

		constexpr Vector4 operator-() const { return { -X, -Y, -Z, -W }; }
		constexpr Vector4& operator+=(const Vector4& other) { X += other.X; Y += other.Y; Z += other.Z; W += other.W; return *this; }
		constexpr Vector4& operator-=(const Vector4& other) { X -= other.X; Y -= other.Y; Z -= other.Z; W -= other.W; return *this; }
		constexpr Vector4& operator*=(float scalar) { X *= scalar; Y *= scalar; Z *= scalar; W *= scalar; return *this; }
		constexpr Vector4& operator/=(float scalar) { X /= scalar; Y /= scalar; Z /= scalar; W /= scalar; return *this; }

		constexpr bool operator==(const Vector4&) const = default;

		friend constexpr Vector4 operator+(const Vector4& a, const Vector4& b) { return { a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W }; }
		friend constexpr Vector4 operator-(const Vector4& a, const Vector4& b) { return { a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.W - b.W }; }
		friend constexpr Vector4 operator*(const Vector4& a, const Vector4& b) { return { a.X * b.X, a.Y * b.Y, a.Z * b.Z, a.W * b.W }; }
		friend constexpr Vector4 operator*(const Vector4& v, float scalar) { return { v.X * scalar, v.Y * scalar, v.Z * scalar, v.W * scalar }; }
		friend constexpr Vector4 operator*(float scalar, const Vector4& v) { return v * scalar; }
		friend constexpr Vector4 operator/(const Vector4& v, float scalar) { return { v.X / scalar, v.Y / scalar, v.Z / scalar, v.W / scalar }; }
	};
}
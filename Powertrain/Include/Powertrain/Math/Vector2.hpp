#pragma once

#include "Powertrain/Math/MathUtilities.hpp"

#include <cmath>

namespace Powertrain
{
	struct Vector2
	{
		float X = 0.0f;
		float Y = 0.0f;

		static constexpr Vector2 Zero() { return { 0.0f, 0.0f }; }
		static constexpr Vector2 One() { return { 1.0f, 1.0f }; }
		static constexpr Vector2 UnitX() { return { 1.0f, 0.0f }; }
		static constexpr Vector2 UnitY() { return { 0.0f, 1.0f }; }

		static constexpr float Dot(const Vector2& a, const Vector2& b) { return a.X * b.X + a.Y * b.Y; }
		static constexpr Vector2 Lerp(const Vector2& a, const Vector2& b, float t) { return { Math::Lerp(a.X, b.X, t), Math::Lerp(a.Y, b.Y, t) }; }
		static constexpr Vector2 Min(const Vector2& a, const Vector2& b) { return { a.X < b.X ? a.X : b.X, a.Y < b.Y ? a.Y : b.Y }; }
		static constexpr Vector2 Max(const Vector2& a, const Vector2& b) { return { a.X > b.X ? a.X : b.X, a.Y > b.Y ? a.Y : b.Y }; }

		constexpr float LengthSquared() const { return X * X + Y * Y; }
		float Length() const { return std::sqrt(LengthSquared()); }

		// Zero stays zero instead of producing NaN
		Vector2 Normalized() const
		{
			const float l_Length = Length();

			return l_Length > Math::k_Epsilon ? Vector2{ X / l_Length, Y / l_Length } : Zero();
		}

		constexpr Vector2 operator-() const { return { -X, -Y }; }
		constexpr Vector2& operator+=(const Vector2& other) { X += other.X; Y += other.Y; return *this; }
		constexpr Vector2& operator-=(const Vector2& other) { X -= other.X; Y -= other.Y; return *this; }
		constexpr Vector2& operator*=(float scalar) { X *= scalar; Y *= scalar; return *this; }
		constexpr Vector2& operator/=(float scalar) { X /= scalar; Y /= scalar; return *this; }

		constexpr bool operator==(const Vector2&) const = default;
	};

	constexpr Vector2 operator+(const Vector2& a, const Vector2& b) { return { a.X + b.X, a.Y + b.Y }; }
	constexpr Vector2 operator-(const Vector2& a, const Vector2& b) { return { a.X - b.X, a.Y - b.Y }; }
	constexpr Vector2 operator*(const Vector2& a, const Vector2& b) { return { a.X * b.X, a.Y * b.Y }; }
	constexpr Vector2 operator*(const Vector2& v, float scalar) { return { v.X * scalar, v.Y * scalar }; }
	constexpr Vector2 operator*(float scalar, const Vector2& v) { return v * scalar; }
	constexpr Vector2 operator/(const Vector2& v, float scalar) { return { v.X / scalar, v.Y / scalar }; }
}
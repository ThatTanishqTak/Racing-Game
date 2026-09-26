#pragma once

#include <cmath>
#include <numbers>

namespace Powertrain
{
	namespace Math
	{
		inline constexpr float k_Pi = std::numbers::pi_v<float>;
		inline constexpr float k_TwoPi = 2.0f * k_Pi;
		inline constexpr float k_HalfPi = 0.5f * k_Pi;
		inline constexpr float k_Epsilon = 1e-6f;

		constexpr float ToRadians(float degrees) { return degrees * (k_Pi / 180.0f); }
		constexpr float ToDegrees(float radians) { return radians * (180.0f / k_Pi); }

		constexpr float Lerp(float a, float b, float t) { return a + (b - a) * t; }
		constexpr float Saturate(float value) { return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); }
		constexpr float Sign(float value) { return value < 0.0f ? -1.0f : (value > 0.0f ? 1.0f : 0.0f); }
		constexpr float Abs(float value) { return value < 0.0f ? -value : value; }

		constexpr bool NearlyEqual(float a, float b, float tolerance = k_Epsilon) { return Abs(a - b) <= tolerance; }
		constexpr bool NearlyZero(float value, float tolerance = k_Epsilon) { return Abs(value) <= tolerance; }

		// Wraps an angle into [-pi, pi)
		inline float WrapAngle(float radians)
		{
			float l_Wrapped = std::fmod(radians + k_Pi, k_TwoPi);
			if (l_Wrapped < 0.0f)
			{
				l_Wrapped += k_TwoPi;
			}

			return l_Wrapped - k_Pi;
		}
	}
}
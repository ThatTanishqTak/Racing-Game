#pragma once

namespace Powertrain
{
	class Timestep
	{
	public:
		constexpr explicit Timestep(double seconds = 0.0) : m_Seconds(seconds) {}

		constexpr double Seconds() const { return m_Seconds; }
		constexpr double Milliseconds() const { return m_Seconds * 1000.0; }
		constexpr float SecondsF() const { return static_cast<float>(m_Seconds); }

	private:
		double m_Seconds;
	};
}
#pragma once

#include <chrono>

namespace Powertrain
{
	class Clock
	{
	public:
		Clock() : m_Start(std::chrono::steady_clock::now()) {}

		double Restart()
		{
			const std::chrono::steady_clock::time_point l_Now = std::chrono::steady_clock::now();
			const double l_Elapsed = std::chrono::duration<double>(l_Now - m_Start).count();
			m_Start = l_Now;

			return l_Elapsed;
		}

		double GetElapsed() const
		{
			return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_Start).count();
		}

	private:
		std::chrono::steady_clock::time_point m_Start;
	};
}
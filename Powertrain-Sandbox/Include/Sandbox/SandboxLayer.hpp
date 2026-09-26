#pragma once

#include "Powertrain/Powertrain.hpp"

#include <array>
#include <cstddef>

class SandboxLayer : public Powertrain::Layer
{
public:
	SandboxLayer() : Powertrain::Layer("SandboxLayer")
	{

	}

	void OnAttach() override;
	void OnDetach() override;
	void OnEvent(Powertrain::Event& event) override;
	void OnFixedUpdate(Powertrain::Timestep fixedStep) override;
	void OnUpdate(Powertrain::Timestep deltaTime) override;
	void OnImGuiRender() override;

private:
	void DrawStatsPanel();

private:
	static constexpr size_t k_FrameHistoryCount = 240;

	double m_ElapsedTime = 0.0;
	uint32_t m_FrameCount = 0;
	uint32_t m_TickCount = 0;

	double m_AverageFrameMilliseconds = 0.0;
	uint32_t m_FramesPerSecond = 0;
	uint32_t m_TicksPerSecond = 0;
	std::array<float, k_FrameHistoryCount> m_FrameHistory = {};
	size_t m_FrameHistoryOffset = 0;

	bool m_ShowStats = true;
	bool m_ShowDemo = false;
};
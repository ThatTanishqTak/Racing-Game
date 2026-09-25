#pragma once

#include "Powertrain/Powertrain.hpp"

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

private:
	double m_ElapsedTime = 0.0;
	uint32_t m_FrameCount = 0;
	uint32_t m_TickCount = 0;
	double m_RunTime = 0.0;
};
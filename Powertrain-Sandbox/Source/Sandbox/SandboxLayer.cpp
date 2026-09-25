#include "Sandbox/SandboxLayer.hpp"

void SandboxLayer::OnAttach()
{
	PT_INFO("{} ATTACHED", GetName());
}

void SandboxLayer::OnEvent(Powertrain::Event& event)
{
	event.Dispatch<Powertrain::KeyEvent>([](const Powertrain::KeyEvent& keyEvent)
	{
		PT_INFO("Key 0x{:02X} {}", static_cast<uint16_t>(keyEvent.Key), keyEvent.Pressed ? "pressed" : "released");

		return false;
	});
}

void SandboxLayer::OnDetach()
{
	PT_INFO("{} detached", GetName());
}

void SandboxLayer::OnFixedUpdate(Powertrain::Timestep)
{
	++m_TickCount;
}

void SandboxLayer::OnUpdate(Powertrain::Timestep deltaTime)
{
	m_RunTime += deltaTime.Seconds();
	if (m_RunTime >= 3.0)
	{
		GetContext().RequestExit();
	}

	m_ElapsedTime += deltaTime.Seconds();
	++m_FrameCount;

	if (m_ElapsedTime >= 1.0)
	{
		PT_TRACE("{} frames, {} ticks in {:.3f} s", m_FrameCount, m_TickCount, m_ElapsedTime);

		m_ElapsedTime = 0.0;
		m_FrameCount = 0;
		m_TickCount = 0;
	}
}
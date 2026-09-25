#include "Sandbox/SandboxLayer.hpp"

void SandboxLayer::OnAttach()
{
	PT_INFO("{} ATTACHED", GetName());
}

void SandboxLayer::OnEvent(Powertrain::Event& event)
{
	event.Dispatch<Powertrain::KeyEvent>([this](const Powertrain::KeyEvent& keyEvent)
	{
		PT_INFO("Key 0x{:02X} {}", static_cast<uint16_t>(keyEvent.Key), keyEvent.Pressed ? "pressed" : "released");

		if (!keyEvent.Pressed || keyEvent.Repeat)
		{
			return false;
		}

		if (keyEvent.Key == Powertrain::KeyCode::V)
		{
			Powertrain::Renderer& l_Renderer = GetContext().GetRenderer();
			l_Renderer.SetVSync(!l_Renderer.IsVSyncEnabled());

			return true;
		}

		if (keyEvent.Key == Powertrain::KeyCode::F11)
		{
			Powertrain::Window& l_Window = GetContext().GetWindow();
			const bool l_Windowed = l_Window.GetMode() == Powertrain::WindowMode::Windowed;
			l_Window.SetMode(l_Windowed ? Powertrain::WindowMode::BorderlessFullscreen : Powertrain::WindowMode::Windowed);

			return true;
		}

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
	m_ElapsedTime += deltaTime.Seconds();
	++m_FrameCount;

	if (m_ElapsedTime >= 1.0)
	{
		const Powertrain::RendererStats& l_Stats = GetContext().GetRenderer().GetStats();
		PT_TRACE("{} frames, {} ticks in {:.3f} s, last frame {:.2f} ms, VSync {}", m_FrameCount, m_TickCount, m_ElapsedTime, l_Stats.CpuFrameMilliseconds, GetContext().GetRenderer().IsVSyncEnabled() ? "on" : "off");

		m_ElapsedTime = 0.0;
		m_FrameCount = 0;
		m_TickCount = 0;
	}
}
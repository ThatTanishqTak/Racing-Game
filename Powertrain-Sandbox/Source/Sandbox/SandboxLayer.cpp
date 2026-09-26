#include "Sandbox/SandboxLayer.hpp"

#include <imgui.h>

void SandboxLayer::OnAttach()
{
	PT_INFO("{} ATTACHED", GetName());
}

void SandboxLayer::OnEvent(Powertrain::Event& event)
{
	event.Dispatch<Powertrain::KeyEvent>([this](const Powertrain::KeyEvent& keyEvent)
	{
		PT_INFO("Key 0x{:02X} {}", static_cast<uint16_t>(keyEvent.Key), keyEvent.Pressed ? "pressed" : "released");

		// A focused text field owns the keyboard, so the toggles below must not fire while typing into a panel
		if (!keyEvent.Pressed || keyEvent.Repeat || ImGui::GetIO().WantCaptureKeyboard)
		{
			return false;
		}

		if (keyEvent.Key == Powertrain::KeyCode::F1)
		{
			m_ShowStats = !m_ShowStats;

			return true;
		}

		if (keyEvent.Key == Powertrain::KeyCode::F2)
		{
			m_ShowDemo = !m_ShowDemo;

			return true;
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

	const Powertrain::RendererStats& l_Stats = GetContext().GetRenderer().GetStats();
	m_FrameHistory[m_FrameHistoryOffset] = static_cast<float>(l_Stats.CpuFrameMilliseconds);
	m_FrameHistoryOffset = (m_FrameHistoryOffset + 1) % k_FrameHistoryCount;
	m_GpuTimeAccumulator += l_Stats.GpuFrameMilliseconds;

	if (m_ElapsedTime >= 1.0)
	{
		m_AverageFrameMilliseconds = m_ElapsedTime * 1000.0 / m_FrameCount;
		m_AverageGpuMilliseconds = m_GpuTimeAccumulator / m_FrameCount;
		m_FramesPerSecond = m_FrameCount;
		m_TicksPerSecond = m_TickCount;

		PT_TRACE("{} frames, {} ticks in {:.3f} s, CPU {:.2f} ms, GPU {:.2f} ms, VSync {}", m_FrameCount, m_TickCount, m_ElapsedTime, m_AverageFrameMilliseconds, m_AverageGpuMilliseconds, GetContext().GetRenderer().IsVSyncEnabled() ? "on" : "off");

		m_ElapsedTime = 0.0;
		m_GpuTimeAccumulator = 0.0;
		m_FrameCount = 0;
		m_TickCount = 0;
	}
}

void SandboxLayer::OnImGuiRender()
{
	if (m_ShowDemo)
	{
		ImGui::ShowDemoWindow(&m_ShowDemo);
	}

	if (m_ShowStats)
	{
		DrawStatsPanel();
	}
}

void SandboxLayer::DrawStatsPanel()
{
	const Powertrain::Renderer& l_Renderer = GetContext().GetRenderer();
	const Powertrain::Window& l_Window = GetContext().GetWindow();
	const Powertrain::RendererStats& l_Stats = l_Renderer.GetStats();
	const std::string_view l_Adapter = l_Renderer.GetAdapterName();

	ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Stats", &m_ShowStats, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("%.*s", static_cast<int>(l_Adapter.size()), l_Adapter.data());
		ImGui::Separator();

		ImGui::Text("CPU frame  %6.2f ms  (%u FPS)", m_AverageFrameMilliseconds, m_FramesPerSecond);
		ImGui::Text("GPU frame  %6.2f ms", m_AverageGpuMilliseconds);
		ImGui::Text("Fixed ticks   %u / s", m_TicksPerSecond);
		ImGui::PlotLines("##FrameTimes", m_FrameHistory.data(), static_cast<int>(k_FrameHistoryCount), static_cast<int>(m_FrameHistoryOffset), nullptr, 0.0f, 33.3f, ImVec2(240.0f, 60.0f));
		ImGui::Separator();

		ImGui::Text("Draw calls %u   Triangles %u", l_Stats.DrawCalls, l_Stats.Triangles);
		ImGui::Text("Frame %llu", static_cast<unsigned long long>(l_Stats.FrameIndex));
		ImGui::Text("Window %ux%u, %s, VSync %s", l_Window.GetWidth(), l_Window.GetHeight(), l_Window.GetMode() == Powertrain::WindowMode::Windowed ? "windowed" : "borderless", l_Renderer.IsVSyncEnabled() ? "on" : "off");
		ImGui::Separator();

		ImGui::TextDisabled("F1 stats  F2 demo  V vsync  F11 fullscreen");
	}
	ImGui::End();
}
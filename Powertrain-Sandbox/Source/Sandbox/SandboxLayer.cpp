#include "Sandbox/SandboxLayer.hpp"

#include <DirectXMath.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	constexpr float k_CameraPitch = DirectX::XMConvertToRadians(25.0f);
	constexpr float k_CameraYawSpeed = 0.3f;
	constexpr float k_CameraZoomStep = 1.0f;
	constexpr float k_CameraMinDistance = 3.0f;
	constexpr float k_CameraMaxDistance = 60.0f;
	constexpr float k_BoxSpinSpeed = 0.8f;
	constexpr int k_GridHalfExtent = 10;

	// Matrix4 shares DirectXMath's row-major layout, so the store is a straight copy
	Powertrain::Matrix4 ToMatrix4(DirectX::FXMMATRIX matrix)
	{
		DirectX::XMFLOAT4X4 l_Stored;
		DirectX::XMStoreFloat4x4(&l_Stored, matrix);

		Powertrain::Matrix4 l_Result;
		std::memcpy(l_Result.M, l_Stored.m, sizeof(l_Result.M));

		return l_Result;
	}
}

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

		if (keyEvent.Key == Powertrain::KeyCode::F3)
		{
			m_ShowDebugScene = !m_ShowDebugScene;

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

	UpdateCamera(deltaTime);

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

void SandboxLayer::OnRender()
{
	if (m_ShowDebugScene)
	{
		DrawDebugScene();
	}
}

void SandboxLayer::UpdateCamera(Powertrain::Timestep deltaTime)
{
	const float l_DeltaSeconds = static_cast<float>(deltaTime.Seconds());

	m_CameraYaw += k_CameraYawSpeed * l_DeltaSeconds;
	m_BoxAngle += k_BoxSpinSpeed * l_DeltaSeconds;
	m_CameraDistance = std::clamp(m_CameraDistance - GetContext().GetInput().GetMouseScroll() * k_CameraZoomStep, k_CameraMinDistance, k_CameraMaxDistance);

	const Powertrain::Window& l_Window = GetContext().GetWindow();
	const float l_Aspect = l_Window.GetHeight() != 0 ? static_cast<float>(l_Window.GetWidth()) / static_cast<float>(l_Window.GetHeight()) : 1.0f;

	// Right-handed, +Y up, orbiting the origin; the plan leans this way for M5
	const DirectX::XMVECTOR l_Eye = DirectX::XMVectorSet(
		m_CameraDistance * std::cos(k_CameraPitch) * std::cos(m_CameraYaw),
		m_CameraDistance * std::sin(k_CameraPitch),
		m_CameraDistance * std::cos(k_CameraPitch) * std::sin(m_CameraYaw),
		1.0f);
	const DirectX::XMVECTOR l_Target = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
	const DirectX::XMVECTOR l_Up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	const DirectX::XMMATRIX l_View = DirectX::XMMatrixLookAtRH(l_Eye, l_Target, l_Up);
	const DirectX::XMMATRIX l_Projection = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(60.0f), l_Aspect, 0.1f, 500.0f);

	// Row vectors: view first, then projection
	GetContext().GetRenderer().SetViewProjection(ToMatrix4(DirectX::XMMatrixMultiply(l_View, l_Projection)));
}

void SandboxLayer::DrawDebugScene()
{
	Powertrain::DebugDraw& l_Draw = GetContext().GetRenderer().GetDebugDraw();

	// Ground grid on XZ, brighter every fifth metre
	for (int l_Index = -k_GridHalfExtent; l_Index <= k_GridHalfExtent; ++l_Index)
	{
		const float l_Offset = static_cast<float>(l_Index);
		const float l_Extent = static_cast<float>(k_GridHalfExtent);
		const bool l_Major = l_Index % 5 == 0;
		const Powertrain::Color l_Color = l_Major ? Powertrain::Color{ 0.45f, 0.45f, 0.50f, 1.0f } : Powertrain::Color{ 0.18f, 0.18f, 0.22f, 1.0f };

		l_Draw.Line({ l_Offset, 0.0f, -l_Extent }, { l_Offset, 0.0f, l_Extent }, l_Color);
		l_Draw.Line({ -l_Extent, 0.0f, l_Offset }, { l_Extent, 0.0f, l_Offset }, l_Color);
	}

	// World axes: X red, Y green, Z blue
	l_Draw.Arrow({ 0.0f, 0.0f, 0.0f }, { 2.0f, 0.0f, 0.0f }, { 1.0f, 0.1f, 0.1f, 1.0f });
	l_Draw.Arrow({ 0.0f, 0.0f, 0.0f }, { 0.0f, 2.0f, 0.0f }, { 0.1f, 1.0f, 0.1f, 1.0f });
	l_Draw.Arrow({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 2.0f }, { 0.1f, 0.3f, 1.0f, 1.0f });

	// A spinning car-sized box and a sphere beside it
	const DirectX::XMMATRIX l_BoxTransform = DirectX::XMMatrixMultiply(DirectX::XMMatrixRotationY(m_BoxAngle), DirectX::XMMatrixTranslation(0.0f, 0.7f, 0.0f));
	l_Draw.Box(ToMatrix4(l_BoxTransform), { 1.0f, 0.7f, 2.2f }, { 1.0f, 0.8f, 0.2f, 1.0f });
	l_Draw.Sphere({ 5.0f, 1.0f, 0.0f }, 1.0f, { 0.3f, 0.9f, 1.0f, 0.8f });
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

		ImGui::TextDisabled("F1 stats  F2 demo  F3 debug scene  wheel zoom  V vsync  F11 fullscreen");
	}
	ImGui::End();
}
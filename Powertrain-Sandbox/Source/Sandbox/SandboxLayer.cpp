#include "Sandbox/SandboxLayer.hpp"

#include "Sandbox/SandboxScene.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace
{
	constexpr float k_CameraPitch = Powertrain::Math::ToRadians(25.0f);
	constexpr float k_CameraYawSpeed = 0.3f;
	constexpr float k_CameraZoomStep = 1.0f;
	constexpr float k_CameraMinDistance = 3.0f;
	constexpr float k_CameraMaxDistance = 60.0f;
	constexpr float k_CameraFovDegrees = 60.0f;
	constexpr float k_CameraNearPlane = 0.1f;
	constexpr int k_GridHalfExtent = 10;

	constexpr std::string_view k_SampleJson = R"({ "name": "Sandbox", "mass": 1200, "rwd": true, "gears": [3.2, 2.1, 1.5, 1.1, 0.9], "aero": { "drag": 0.32 } })";
}

void SandboxLayer::OnAttach()
{
	PT_INFO("{} ATTACHED", GetName());

	Powertrain::SceneManager& l_Scenes = GetContext().GetSceneManager();
	m_Scene = &l_Scenes.CreateScene("Sandbox", GetContext());
	l_Scenes.SetActiveScene(*m_Scene);

	m_Scene->AddSystem<MotionSystem>(Powertrain::SystemStage::PrePhysics);
	BuildScene();

	ParseSampleJson();
}

void SandboxLayer::OnDetach()
{
	if (m_Scene != nullptr)
	{
		GetContext().GetSceneManager().DestroyScene(*m_Scene);
		m_Scene = nullptr;
	}

	PT_INFO("{} detached", GetName());
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

		if (keyEvent.Key == Powertrain::KeyCode::Delete)
		{
			if (m_Scene->IsAlive(m_Car))
			{
				m_Scene->DestroyEntity(m_Car);
			}

			return true;
		}

		if (keyEvent.Key == Powertrain::KeyCode::R)
		{
			if (!m_Scene->IsAlive(m_Car))
			{
				BuildScene();
			}

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

void SandboxLayer::BuildScene()
{
	using namespace Powertrain;

	// A car-sized box spinning on the spot
	m_Car = m_Scene->CreateEntity("Car");
	m_Scene->GetRegistry().Get<TransformComponent>(m_Car).Teleport({ 0.0f, 0.7f, 0.0f });
	m_Scene->GetRegistry().Add<DebugShapeComponent>(m_Car, DebugShapeComponent::Kind::Box, Vector3{ 1.0f, 0.7f, 2.2f }, 0.0f, Color{ 1.0f, 0.8f, 0.2f, 1.0f });
	m_Scene->GetRegistry().Add<MotionComponent>(m_Car, 0.8f);

	// A roof light that bobs in the car's frame, so it orbits with the spin and rises and falls on its own
	const Entity l_RoofLight = m_Scene->CreateEntity("RoofLight");
	m_Scene->SetParent(l_RoofLight, m_Car);
	m_Scene->GetRegistry().Get<TransformComponent>(l_RoofLight).Teleport({ 0.0f, 1.0f, 0.0f });
	m_Scene->GetRegistry().Add<DebugShapeComponent>(l_RoofLight, DebugShapeComponent::Kind::Sphere, Vector3::Zero(), 0.25f, Color{ 1.0f, 0.3f, 0.3f, 1.0f });
	m_Scene->GetRegistry().Add<MotionComponent>(l_RoofLight, 0.0f, 1.0f, 0.15f, 1.0f);

	// A thin antenna at the rear corner spinning fast about its own axis, composed with the car's spin
	const Entity l_Antenna = m_Scene->CreateEntity("Antenna");
	m_Scene->SetParent(l_Antenna, m_Car);
	m_Scene->GetRegistry().Get<TransformComponent>(l_Antenna).Teleport({ 0.6f, 1.1f, -1.8f });
	m_Scene->GetRegistry().Add<DebugShapeComponent>(l_Antenna, DebugShapeComponent::Kind::Box, Vector3{ 0.05f, 0.4f, 0.2f }, 0.0f, Color{ 0.4f, 1.0f, 0.4f, 1.0f });
	m_Scene->GetRegistry().Add<MotionComponent>(l_Antenna, 4.0f);

	// A free-standing sphere bobbing slowly beside the car
	const Entity l_Sphere = m_Scene->CreateEntity("Sphere");
	m_Scene->GetRegistry().Get<TransformComponent>(l_Sphere).Teleport({ 5.0f, 1.0f, 0.0f });
	m_Scene->GetRegistry().Add<DebugShapeComponent>(l_Sphere, DebugShapeComponent::Kind::Sphere, Vector3::Zero(), 1.0f, Color{ 0.3f, 0.9f, 1.0f, 0.8f });
	m_Scene->GetRegistry().Add<MotionComponent>(l_Sphere, 0.0f, 1.0f, 0.5f, 0.5f, Math::k_HalfPi);

	PT_INFO("Sandbox scene built with {} entities", m_Scene->GetRegistry().GetAliveCount());
}

void SandboxLayer::ParseSampleJson()
{
	Powertrain::JsonParser l_Parser;
	Powertrain::JsonValue l_Document;
	if (!l_Parser.Parse(k_SampleJson, l_Document))
	{
		PT_ERROR("Sample JSON failed to parse: {}", l_Parser.GetError());

		return;
	}

	const Powertrain::JsonValue* l_Gears = l_Document.Find("gears");
	PT_INFO("Sample JSON: name '{}', mass {} kg, rwd {}, {} gears, first ratio {:.2f}, drag {:.2f}", l_Document.GetString("name", "?"), l_Document.GetInteger("mass", 0), l_Document.GetBool("rwd", false), l_Gears != nullptr ? l_Gears->Size() : 0, l_Gears != nullptr && l_Gears->At(0) != nullptr ? l_Gears->At(0)->AsNumber() : 0.0, l_Document.Find("aero") != nullptr ? l_Document.Find("aero")->GetNumber("drag", 0.0) : 0.0);
}

void SandboxLayer::UpdateCamera(Powertrain::Timestep deltaTime)
{
	const float l_DeltaSeconds = deltaTime.SecondsF();

	m_CameraYaw += k_CameraYawSpeed * l_DeltaSeconds;
	m_CameraDistance = std::clamp(m_CameraDistance - GetContext().GetInput().GetMouseScroll() * k_CameraZoomStep, k_CameraMinDistance, k_CameraMaxDistance);

	const Powertrain::Window& l_Window = GetContext().GetWindow();
	const float l_Aspect = l_Window.GetHeight() != 0 ? static_cast<float>(l_Window.GetWidth()) / static_cast<float>(l_Window.GetHeight()) : 1.0f;

	// Right-handed, +Y up, orbiting a point above the origin; reversed-Z infinite projection needs no far plane
	const Powertrain::Vector3 l_Eye =
	{
		m_CameraDistance * std::cos(k_CameraPitch) * std::cos(m_CameraYaw),
		m_CameraDistance * std::sin(k_CameraPitch),
		m_CameraDistance * std::cos(k_CameraPitch) * std::sin(m_CameraYaw)
	};

	const Powertrain::Matrix4 l_View = Powertrain::Matrix4::LookAt(l_Eye, { 0.0f, 1.0f, 0.0f }, Powertrain::Vector3::Up());
	const Powertrain::Matrix4 l_Projection = Powertrain::Matrix4::Perspective(Powertrain::Math::ToRadians(k_CameraFovDegrees), l_Aspect, k_CameraNearPlane);

	// Row vectors: view first, then projection
	GetContext().GetRenderer().SetViewProjection(l_View * l_Projection);
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

	// Every entity with a shape, at the interpolated world transform TransformSystem wrote in PreRender
	m_Scene->GetRegistry().Each<Powertrain::WorldTransformComponent, DebugShapeComponent>([&l_Draw](Powertrain::Entity, const Powertrain::WorldTransformComponent& world, const DebugShapeComponent& shape)
	{
		if (shape.Shape == DebugShapeComponent::Kind::Box)
		{
			l_Draw.Box(world.World, shape.HalfExtents, shape.Tint);
		}
		else
		{
			l_Draw.Sphere(world.World.GetTranslation(), shape.Radius, shape.Tint);
		}
	});
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
		ImGui::Text("Scene '%s'   Entities %u   Alpha %.2f", m_Scene->GetName().c_str(), m_Scene->GetRegistry().GetAliveCount(), m_Scene->GetInterpolationAlpha());
		ImGui::Text("Window %ux%u, %s, VSync %s", l_Window.GetWidth(), l_Window.GetHeight(), l_Window.GetMode() == Powertrain::WindowMode::Windowed ? "windowed" : "borderless", l_Renderer.IsVSyncEnabled() ? "on" : "off");
		ImGui::Separator();

		ImGui::TextDisabled("F1 stats  F2 demo  F3 debug scene  wheel zoom  V vsync  F11 fullscreen");
		ImGui::TextDisabled("Delete destroys the car and its children  R rebuilds");
	}
	ImGui::End();
}
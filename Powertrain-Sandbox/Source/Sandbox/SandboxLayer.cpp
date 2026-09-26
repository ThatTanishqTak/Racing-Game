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
	constexpr float k_GroundHalfSize = 10.0f;
	constexpr uint32_t k_CheckerSize = 512;
	constexpr uint32_t k_CheckerCell = 64;

	// Where the light comes from, in the direction it travels
	constexpr Powertrain::Vector3 k_SunDirection = { -0.3f, -1.0f, -0.2f };

	// The animated sun circles the scene while its elevation breathes between the two limits
	constexpr float k_SunAngularSpeed = 0.25f;
	constexpr float k_SunMinElevation = Powertrain::Math::ToRadians(8.0f);
	constexpr float k_SunMaxElevation = Powertrain::Math::ToRadians(60.0f);

	// Lines sit a hair above the ground plane so the depth test does not flicker between them
	constexpr float k_GridHeight = 0.01f;

	constexpr std::string_view k_SampleJson = R"({ "name": "Sandbox", "mass": 1200, "rwd": true, "gears": [3.2, 2.1, 1.5, 1.1, 0.9], "aero": { "drag": 0.32 } })";
}

void SandboxLayer::OnAttach()
{
	PT_INFO("{} ATTACHED", GetName());

	Powertrain::SceneManager& l_Scenes = GetContext().GetSceneManager();
	m_Scene = &l_Scenes.CreateScene("Sandbox", GetContext());
	l_Scenes.SetActiveScene(*m_Scene);

	m_Scene->AddSystem<MotionSystem>(Powertrain::SystemStage::PrePhysics);
	CreateResources();
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

	DestroyResources();

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
			m_ShowDebugOverlay = !m_ShowDebugOverlay;

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
				BuildCar();
			}

			return true;
		}

		if (keyEvent.Key == Powertrain::KeyCode::L)
		{
			m_AnimateSun = !m_AnimateSun;

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
	UpdateSun(deltaTime);

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
	if (m_ShowDebugOverlay)
	{
		DrawDebugOverlay();
	}
}

void SandboxLayer::CreateResources()
{
	Powertrain::Renderer& l_Renderer = GetContext().GetRenderer();

	m_BoxMesh = l_Renderer.CreateMesh(Powertrain::MeshPrimitives::MakeBox({ 0.5f, 0.5f, 0.5f }));
	m_SphereMesh = l_Renderer.CreateMesh(Powertrain::MeshPrimitives::MakeSphere(1.0f));
	m_PlaneMesh = l_Renderer.CreateMesh(Powertrain::MeshPrimitives::MakePlane(k_GroundHalfSize, k_GroundHalfSize));

	m_CheckerTexture = l_Renderer.CreateTexture(Powertrain::TexturePrimitives::MakeChecker(k_CheckerSize, k_CheckerCell, { 0.62f, 0.62f, 0.60f, 1.0f }, { 0.30f, 0.30f, 0.32f, 1.0f }));

	// Rough asphalt under the checker
	Powertrain::MaterialDescription l_Ground;
	l_Ground.Roughness = 0.9f;
	l_Ground.BaseColorTexture = m_CheckerTexture;
	m_GroundMaterial = l_Renderer.CreateMaterial(l_Ground);

	// Painted metal for the body
	Powertrain::MaterialDescription l_Body;
	l_Body.BaseColor = { 0.72f, 0.07f, 0.05f, 1.0f };
	l_Body.Metallic = 0.9f;
	l_Body.Roughness = 0.35f;
	m_BodyMaterial = l_Renderer.CreateMaterial(l_Body);

	// The roof light glows on its own
	Powertrain::MaterialDescription l_Light;
	l_Light.BaseColor = { 1.0f, 0.5f, 0.1f, 1.0f };
	l_Light.Emissive = { 4.0f, 1.6f, 0.3f, 1.0f };
	l_Light.Roughness = 0.6f;
	m_LightMaterial = l_Renderer.CreateMaterial(l_Light);

	// Dark rubber for the antenna
	Powertrain::MaterialDescription l_Antenna;
	l_Antenna.BaseColor = { 0.08f, 0.08f, 0.09f, 1.0f };
	l_Antenna.Roughness = 0.7f;
	m_AntennaMaterial = l_Renderer.CreateMaterial(l_Antenna);

	// Blue plastic for the free sphere, where the GGX highlight is easiest to read
	Powertrain::MaterialDescription l_Sphere;
	l_Sphere.BaseColor = { 0.15f, 0.40f, 0.90f, 1.0f };
	l_Sphere.Roughness = 0.4f;
	m_SphereMaterial = l_Renderer.CreateMaterial(l_Sphere);

	PT_INFO("Sandbox resources created: meshes {} {} {}, checker {}, materials {} {} {} {} {}", m_BoxMesh.IsValid(), m_SphereMesh.IsValid(), m_PlaneMesh.IsValid(), m_CheckerTexture.IsValid(), m_GroundMaterial.IsValid(), m_BodyMaterial.IsValid(), m_LightMaterial.IsValid(), m_AntennaMaterial.IsValid(), m_SphereMaterial.IsValid());
}

void SandboxLayer::DestroyResources()
{
	Powertrain::Renderer& l_Renderer = GetContext().GetRenderer();

	l_Renderer.DestroyMaterial(m_SphereMaterial);
	l_Renderer.DestroyMaterial(m_AntennaMaterial);
	l_Renderer.DestroyMaterial(m_LightMaterial);
	l_Renderer.DestroyMaterial(m_BodyMaterial);
	l_Renderer.DestroyMaterial(m_GroundMaterial);
	l_Renderer.DestroyTexture(m_CheckerTexture);
	l_Renderer.DestroyMesh(m_PlaneMesh);
	l_Renderer.DestroyMesh(m_SphereMesh);
	l_Renderer.DestroyMesh(m_BoxMesh);

	m_SphereMaterial = Powertrain::MaterialHandle();
	m_AntennaMaterial = Powertrain::MaterialHandle();
	m_LightMaterial = Powertrain::MaterialHandle();
	m_BodyMaterial = Powertrain::MaterialHandle();
	m_GroundMaterial = Powertrain::MaterialHandle();
	m_CheckerTexture = Powertrain::TextureHandle();
	m_PlaneMesh = Powertrain::MeshHandle();
	m_SphereMesh = Powertrain::MeshHandle();
	m_BoxMesh = Powertrain::MeshHandle();
}

void SandboxLayer::BuildScene()
{
	// The orbit camera is an entity like anything else; UpdateCamera teleports it every frame
	m_Camera = m_Scene->CreateEntity("Camera");
	m_Scene->GetRegistry().Add<Powertrain::CameraComponent>(m_Camera, k_CameraFovDegrees, k_CameraNearPlane);
	m_Scene->SetPrimaryCamera(m_Camera);

	// The sun is an entity too: it shines along its -Z, so LookRotation aims it down the travel direction
	m_Sun = m_Scene->CreateEntity("Sun");
	m_Scene->GetRegistry().Get<Powertrain::TransformComponent>(m_Sun).Teleport({ 0.0f, 10.0f, 0.0f }, Powertrain::Quaternion::LookRotation(k_SunDirection, Powertrain::Vector3::Up()));
	m_Scene->GetRegistry().Add<Powertrain::DirectionalLightComponent>(m_Sun, Powertrain::Color{ 1.0f, 0.96f, 0.90f, 1.0f }, 8.0f);

	// Ground the grid sits on, so the depth test has something to hide the meshes behind
	const Powertrain::Entity l_Ground = m_Scene->CreateEntity("Ground");
	m_Scene->GetRegistry().Add<Powertrain::MeshRendererComponent>(l_Ground, m_PlaneMesh, m_GroundMaterial);

	// A free-standing unit sphere bobbing slowly beside the car
	const Powertrain::Entity l_Sphere = m_Scene->CreateEntity("Sphere");
	m_Scene->GetRegistry().Get<Powertrain::TransformComponent>(l_Sphere).Teleport({ 5.0f, 1.0f, 0.0f });
	m_Scene->GetRegistry().Add<Powertrain::MeshRendererComponent>(l_Sphere, m_SphereMesh, m_SphereMaterial);
	m_Scene->GetRegistry().Add<MotionComponent>(l_Sphere, 0.0f, 1.0f, 0.5f, 0.5f, Powertrain::Math::k_HalfPi);

	BuildCar();

	PT_INFO("Sandbox scene built with {} entities", m_Scene->GetRegistry().GetAliveCount());
}

void SandboxLayer::BuildCar()
{
	// The car root spins on the spot and stays unscaled, so its children keep their own proportions
	m_Car = m_Scene->CreateEntity("Car");
	m_Scene->GetRegistry().Get<Powertrain::TransformComponent>(m_Car).Teleport({ 0.0f, 0.7f, 0.0f });
	m_Scene->GetRegistry().Add<MotionComponent>(m_Car, 0.8f);

	// The body is the unit box stretched to car size
	const Powertrain::Entity l_Body = m_Scene->CreateEntity("Body");
	m_Scene->SetParent(l_Body, m_Car);
	m_Scene->GetRegistry().Get<Powertrain::TransformComponent>(l_Body).Scale = { 2.0f, 1.4f, 4.4f };
	m_Scene->GetRegistry().Add<Powertrain::MeshRendererComponent>(l_Body, m_BoxMesh, m_BodyMaterial);

	// A roof light that bobs in the car's frame, so it orbits with the spin and rises and falls on its own
	const Powertrain::Entity l_RoofLight = m_Scene->CreateEntity("RoofLight");
	m_Scene->SetParent(l_RoofLight, m_Car);
	m_Scene->GetRegistry().Get<Powertrain::TransformComponent>(l_RoofLight).Teleport({ 0.0f, 1.0f, 0.0f });
	m_Scene->GetRegistry().Get<Powertrain::TransformComponent>(l_RoofLight).Scale = { 0.25f, 0.25f, 0.25f };
	m_Scene->GetRegistry().Add<Powertrain::MeshRendererComponent>(l_RoofLight, m_SphereMesh, m_LightMaterial);
	m_Scene->GetRegistry().Add<MotionComponent>(l_RoofLight, 0.0f, 1.0f, 0.15f, 1.0f);

	// A thin antenna at the rear corner spinning fast about its own axis, composed with the car's spin
	const Powertrain::Entity l_Antenna = m_Scene->CreateEntity("Antenna");
	m_Scene->SetParent(l_Antenna, m_Car);
	m_Scene->GetRegistry().Get<Powertrain::TransformComponent>(l_Antenna).Teleport({ 0.6f, 1.1f, -1.8f });
	m_Scene->GetRegistry().Get<Powertrain::TransformComponent>(l_Antenna).Scale = { 0.1f, 0.8f, 0.4f };
	m_Scene->GetRegistry().Add<Powertrain::MeshRendererComponent>(l_Antenna, m_BoxMesh, m_AntennaMaterial);
	m_Scene->GetRegistry().Add<MotionComponent>(l_Antenna, 4.0f);
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

	// Right-handed, +Y up, orbiting a point above the origin
	const Powertrain::Vector3 l_Eye =
	{
		m_CameraDistance * std::cos(k_CameraPitch) * std::cos(m_CameraYaw),
		m_CameraDistance * std::sin(k_CameraPitch),
		m_CameraDistance * std::cos(k_CameraPitch) * std::sin(m_CameraYaw)
	};

	// The camera entity's transform is the pose; SceneRenderer inverts it into the view and builds the projection from the
	// CameraComponent and the swap chain aspect. Teleport sets both states so the camera never lags a tick behind
	const Powertrain::Vector3 l_Target = { 0.0f, 1.0f, 0.0f };
	if (m_Scene->IsAlive(m_Camera))
	{
		m_Scene->GetRegistry().Get<Powertrain::TransformComponent>(m_Camera).Teleport(l_Eye, Powertrain::Quaternion::LookRotation(l_Target - l_Eye, Powertrain::Vector3::Up()));
	}
}

void SandboxLayer::UpdateSun(Powertrain::Timestep deltaTime)
{
	if (!m_AnimateSun || !m_Scene->IsAlive(m_Sun))
	{
		return;
	}

	m_SunAngle += k_SunAngularSpeed * deltaTime.SecondsF();

	// The travel direction, aimed the same way BuildScene aims the static sun
	const float l_Elevation = Powertrain::Math::Lerp(k_SunMinElevation, k_SunMaxElevation, 0.5f + 0.5f * std::sin(m_SunAngle * 0.5f));
	const Powertrain::Vector3 l_Travel = { -std::cos(l_Elevation) * std::cos(m_SunAngle), -std::sin(l_Elevation), -std::cos(l_Elevation) * std::sin(m_SunAngle) };
	m_Scene->GetRegistry().Get<Powertrain::TransformComponent>(m_Sun).Teleport({ 0.0f, 10.0f, 0.0f }, Powertrain::Quaternion::LookRotation(l_Travel, Powertrain::Vector3::Up()));
}

void SandboxLayer::DrawDebugOverlay()
{
	Powertrain::DebugDraw& l_Draw = GetContext().GetRenderer().GetDebugDraw();

	// Ground grid on XZ just above the plane mesh, brighter every fifth metre
	for (int l_Index = -k_GridHalfExtent; l_Index <= k_GridHalfExtent; ++l_Index)
	{
		const float l_Offset = static_cast<float>(l_Index);
		const float l_Extent = static_cast<float>(k_GridHalfExtent);
		const bool l_Major = l_Index % 5 == 0;
		const Powertrain::Color l_Color = l_Major ? Powertrain::Color{ 0.45f, 0.45f, 0.50f, 1.0f } : Powertrain::Color{ 0.18f, 0.18f, 0.22f, 1.0f };

		l_Draw.Line({ l_Offset, k_GridHeight, -l_Extent }, { l_Offset, k_GridHeight, l_Extent }, l_Color);
		l_Draw.Line({ -l_Extent, k_GridHeight, l_Offset }, { l_Extent, k_GridHeight, l_Offset }, l_Color);
	}

	// World axes: X red, Y green, Z blue
	l_Draw.Arrow({ 0.0f, k_GridHeight, 0.0f }, { 2.0f, 0.0f, 0.0f }, { 1.0f, 0.1f, 0.1f, 1.0f });
	l_Draw.Arrow({ 0.0f, k_GridHeight, 0.0f }, { 0.0f, 2.0f, 0.0f }, { 0.1f, 1.0f, 0.1f, 1.0f });
	l_Draw.Arrow({ 0.0f, k_GridHeight, 0.0f }, { 0.0f, 0.0f, 2.0f }, { 0.1f, 0.3f, 1.0f, 1.0f });
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

		// Per-pass GPU times for the last whole frame; the environment bake reads zero on the frames it skips
		ImGui::Text("Shadow %5.2f  Forward %5.2f  Sky %5.2f ms", l_Stats.GpuShadowMilliseconds, l_Stats.GpuForwardMilliseconds, l_Stats.GpuSkyMilliseconds);
		ImGui::Text("Lines  %5.2f  Post    %5.2f  Bake %5.2f ms (%u bakes)", l_Stats.GpuDebugDrawMilliseconds, l_Stats.GpuPostMilliseconds, l_Stats.GpuEnvironmentMilliseconds, l_Stats.EnvironmentBakes);
		ImGui::Separator();

		ImGui::Text("Draw calls %u (%u shadow)   Triangles %u   MSAA %ux", l_Stats.DrawCalls, l_Stats.ShadowDrawCalls, l_Stats.Triangles, l_Stats.SampleCount);
		ImGui::Text("Instances %u drawn, %u culled", l_Stats.Instances, l_Stats.CulledInstances);
		ImGui::Text("Meshes %u   Textures %u   Materials %u   GPU %llu KB", l_Stats.Meshes, l_Stats.Textures, l_Stats.Materials, static_cast<unsigned long long>(l_Stats.GpuMemoryBytes / 1024));
		ImGui::Text("Frame %llu", static_cast<unsigned long long>(l_Stats.FrameIndex));
		ImGui::Text("Scene '%s'   Entities %u   Alpha %.2f", m_Scene->GetName().c_str(), m_Scene->GetRegistry().GetAliveCount(), m_Scene->GetInterpolationAlpha());
		ImGui::Text("Window %ux%u, %s, VSync %s", l_Window.GetWidth(), l_Window.GetHeight(), l_Window.GetMode() == Powertrain::WindowMode::Windowed ? "windowed" : "borderless", l_Renderer.IsVSyncEnabled() ? "on" : "off");
		ImGui::Separator();

		ImGui::TextDisabled("F1 stats  F2 demo  F3 grid and axes  wheel zoom  V vsync  F11 fullscreen");
		ImGui::TextDisabled("Delete destroys the car and its children  R rebuilds  L swings the sun");
	}
	ImGui::End();
}
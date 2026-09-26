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
	void OnRender() override;
	void OnImGuiRender() override;

private:
	void CreateResources();
	void DestroyResources();
	void BuildScene();
	void BuildCar();
	void ParseSampleJson();
	void UpdateCamera(Powertrain::Timestep deltaTime);
	void DrawDebugOverlay();
	void DrawStatsPanel();

private:
	static constexpr size_t k_FrameHistoryCount = 240;

	double m_ElapsedTime = 0.0;
	uint32_t m_FrameCount = 0;
	uint32_t m_TickCount = 0;

	double m_AverageFrameMilliseconds = 0.0;
	double m_GpuTimeAccumulator = 0.0;
	double m_AverageGpuMilliseconds = 0.0;
	uint32_t m_FramesPerSecond = 0;
	uint32_t m_TicksPerSecond = 0;
	std::array<float, k_FrameHistoryCount> m_FrameHistory = {};
	size_t m_FrameHistoryOffset = 0;

	// Orbit camera entity; the wheel zooms, the yaw advances every frame
	float m_CameraYaw = 0.0f;
	float m_CameraDistance = 14.0f;
	Powertrain::Entity m_Camera;

	// Unit meshes scaled through TransformComponent, so three meshes dress every entity
	Powertrain::MeshHandle m_BoxMesh;
	Powertrain::MeshHandle m_SphereMesh;
	Powertrain::MeshHandle m_PlaneMesh;

	// A procedural checker for the ground and one material per surface
	Powertrain::TextureHandle m_CheckerTexture;
	Powertrain::MaterialHandle m_GroundMaterial;
	Powertrain::MaterialHandle m_BodyMaterial;
	Powertrain::MaterialHandle m_LightMaterial;
	Powertrain::MaterialHandle m_AntennaMaterial;
	Powertrain::MaterialHandle m_SphereMaterial;

	// The entity scene: a sun, a ground plane, a spinning car with a body and two children, a bobbing sphere beside it
	Powertrain::Scene* m_Scene = nullptr;
	Powertrain::Entity m_Sun;
	Powertrain::Entity m_Car;

	bool m_ShowStats = true;
	bool m_ShowDemo = false;
	bool m_ShowDebugOverlay = true;
};
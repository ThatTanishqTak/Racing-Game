#pragma once

#include "Powertrain/Powertrain.hpp"

#include <cstdint>

// Debug-draw stand-in for a mesh
struct DebugShapeComponent
{
	enum class Kind : uint8_t
	{
		Box,
		Sphere
	};

	Kind Shape = Kind::Box;
	Powertrain::Vector3 HalfExtents = { 0.5f, 0.5f, 0.5f };
	float Radius = 0.5f;
	Powertrain::Color Tint;
};

// Spins about the local Y axis and bobs along it; driven at 240 Hz by MotionSystem
struct MotionComponent
{
	float SpinSpeed = 0.0f;
	float BaseHeight = 0.0f;
	float BobAmplitude = 0.0f;
	float BobFrequency = 0.0f;
	float Phase = 0.0f;
};

// PrePhysics game system: writes Position and Rotation each tick, the engine interpolates them for display
class MotionSystem final : public Powertrain::System
{
public:
	void OnFixedUpdate(Powertrain::Scene& scene, Powertrain::Timestep fixedStep) override;

	double GetElapsedSeconds() const { return m_ElapsedSeconds; }

private:
	double m_ElapsedSeconds = 0.0;
};
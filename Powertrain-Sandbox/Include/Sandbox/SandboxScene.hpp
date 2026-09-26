#pragma once

#include "Powertrain/Powertrain.hpp"

#include <cstdint>

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
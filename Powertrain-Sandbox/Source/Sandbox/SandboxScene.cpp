#include "Sandbox/SandboxScene.hpp"

#include <cmath>

void MotionSystem::OnFixedUpdate(Powertrain::Scene& scene, Powertrain::Timestep fixedStep)
{
	using namespace Powertrain;

	m_ElapsedSeconds += fixedStep.Seconds();

	const float l_DeltaSeconds = fixedStep.SecondsF();
	const float l_Time = static_cast<float>(m_ElapsedSeconds);

	scene.GetRegistry().Each<TransformComponent, MotionComponent>([l_DeltaSeconds, l_Time](Entity, TransformComponent& transform, MotionComponent& motion)
	{
		if (motion.SpinSpeed != 0.0f)
		{
			// Renormalized so 240 small steps a second do not drift off the unit sphere
			transform.Rotation = (transform.Rotation * Quaternion::FromAxisAngle(Vector3::UnitY(), motion.SpinSpeed * l_DeltaSeconds)).Normalized();
		}

		if (motion.BobAmplitude != 0.0f)
		{
			transform.Position.Y = motion.BaseHeight + motion.BobAmplitude * std::sin(Math::k_TwoPi * motion.BobFrequency * l_Time + motion.Phase);
		}
	});
}
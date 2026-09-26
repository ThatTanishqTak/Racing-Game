#include "Sandbox/SandboxScene.hpp"

#include <cmath>

void MotionSystem::OnFixedUpdate(Powertrain::Scene& scene, Powertrain::Timestep fixedStep)
{
	m_ElapsedSeconds += fixedStep.Seconds();

	const float l_DeltaSeconds = fixedStep.SecondsF();
	const float l_Time = static_cast<float>(m_ElapsedSeconds);

	scene.GetRegistry().Each<Powertrain::TransformComponent, MotionComponent>([l_DeltaSeconds, l_Time](Powertrain::Entity, Powertrain::TransformComponent& transform, MotionComponent& motion)
	{
		if (motion.SpinSpeed != 0.0f)
		{
			// Renormalized so 240 small steps a second do not drift off the unit sphere
			transform.Rotation = (transform.Rotation * Powertrain::Quaternion::FromAxisAngle(Powertrain::Vector3::UnitY(), motion.SpinSpeed * l_DeltaSeconds)).Normalized();
		}

		if (motion.BobAmplitude != 0.0f)
		{
			transform.Position.Y = motion.BaseHeight + motion.BobAmplitude * std::sin(Powertrain::Math::k_TwoPi * motion.BobFrequency * l_Time + motion.Phase);
		}
	});
}
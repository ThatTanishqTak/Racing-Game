#pragma once

#include "Powertrain/Math/Matrix4.hpp"
#include "Powertrain/Math/Quaternion.hpp"
#include "Powertrain/Math/Vector3.hpp"

namespace Powertrain
{
	// Position, rotation and scale as a value type; ToMatrix builds the local-to-parent matrix
	struct Transform
	{
		Vector3 Position;
		Quaternion Rotation;
		Vector3 Scale = Vector3::One();

		static constexpr Transform Identity() { return {}; }

		// Position and scale linearly, rotation along the shortest arc
		static Transform Lerp(const Transform& a, const Transform& b, float t)
		{
			return { Vector3::Lerp(a.Position, b.Position, t), Quaternion::Slerp(a.Rotation, b.Rotation, t), Vector3::Lerp(a.Scale, b.Scale, t) };
		}

		Matrix4 ToMatrix() const { return Matrix4::TRS(Position, Rotation, Scale); }

		// Scale, rotate, translate a local point into the parent space
		Vector3 TransformPoint(const Vector3& point) const { return Rotation.Rotate(point * Scale) + Position; }
		Vector3 TransformDirection(const Vector3& direction) const { return Rotation.Rotate(direction); }

		constexpr bool operator==(const Transform&) const = default;
	};
}
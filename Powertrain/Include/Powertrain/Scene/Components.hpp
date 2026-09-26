#pragma once

#include "Powertrain/ECS/Entity.hpp"
#include "Powertrain/Math/Matrix4.hpp"
#include "Powertrain/Math/Quaternion.hpp"
#include "Powertrain/Math/Vector3.hpp"

#include <string>

namespace Powertrain
{
	struct NameComponent
	{
		std::string Name;
	};

	// Local transform, relative to the parent. Fixed-step systems write Position and Rotation; the scene copies both into the Previous fields at the start of every tick, and TransformSystem blends the two with the interpolation alpha in PreRender
	struct TransformComponent
	{
		Vector3 Position;
		Quaternion Rotation;
		Vector3 Scale = Vector3::One();
		Vector3 PreviousPosition;
		Quaternion PreviousRotation;

		// Sets both states so the next frame does not interpolate from the old place
		void Teleport(const Vector3& position, const Quaternion& rotation)
		{
			Position = position;
			PreviousPosition = position;
			Rotation = rotation;
			PreviousRotation = rotation;
		}

		void Teleport(const Vector3& position) { Teleport(position, Rotation); }
		void SnapPrevious() { PreviousPosition = Position; PreviousRotation = Rotation; }
	};

	// Intrusive child list; Scene::SetParent keeps the links consistent. A root has an invalid Parent
	struct HierarchyComponent
	{
		Entity Parent;
		Entity FirstChild;
		Entity NextSibling;
	};

	// Interpolated local-to-world matrix written by TransformSystem in PreRender; read in OnRender and by the M6 SceneRenderer
	struct WorldTransformComponent
	{
		Matrix4 World;
	};

	// Reversed-Z infinite projection needs no far plane
	struct CameraComponent
	{
		float VerticalFovDegrees = 60.0f;
		float NearPlane = 0.1f;
	};
}
#pragma once

#include "Powertrain/Math/BoundingBox.hpp"
#include "Powertrain/Math/MathUtilities.hpp"
#include "Powertrain/Math/Matrix4.hpp"
#include "Powertrain/Math/Vector3.hpp"

#include <array>
#include <cstddef>

namespace Powertrain
{
	struct Plane
	{
		Vector3 Normal;
		float Distance = 0.0f;
	};

	struct Frustum
	{
		static constexpr size_t k_PlaneCount = 6;

		std::array<Plane, k_PlaneCount> Planes;

		static Frustum FromViewProjection(const Matrix4& viewProjection)
		{
			const auto l_Column = [&viewProjection](int index)
			{
				return Vector4{ viewProjection.M[0][index], viewProjection.M[1][index], viewProjection.M[2][index], viewProjection.M[3][index] };
			};

			const Vector4 l_X = l_Column(0);
			const Vector4 l_Y = l_Column(1);
			const Vector4 l_Z = l_Column(2);
			const Vector4 l_W = l_Column(3);

			const std::array<Vector4, k_PlaneCount> l_Raw =
			{
				l_W + l_X,
				l_W - l_X,
				l_W + l_Y,
				l_W - l_Y,
				l_W - l_Z,
				l_Z
			};

			Frustum l_Result;
			for (size_t l_Index = 0; l_Index < k_PlaneCount; ++l_Index)
			{
				const Vector3 l_Normal = l_Raw[l_Index].XYZ();
				const float l_Length = l_Normal.Length();
				const float l_Scale = l_Length > Math::k_Epsilon ? 1.0f / l_Length : 0.0f;

				l_Result.Planes[l_Index] = { l_Normal * l_Scale, l_Raw[l_Index].W * l_Scale };
			}

			return l_Result;
		}

		bool Intersects(const BoundingBox& box) const
		{
			const Vector3 l_Center = box.GetCenter();
			const Vector3 l_Extents = box.GetExtents();

			for (const Plane& l_Plane : Planes)
			{
				const float l_Radius = Math::Abs(l_Plane.Normal.X) * l_Extents.X + Math::Abs(l_Plane.Normal.Y) * l_Extents.Y + Math::Abs(l_Plane.Normal.Z) * l_Extents.Z;
				if (Vector3::Dot(l_Plane.Normal, l_Center) + l_Plane.Distance + l_Radius < 0.0f)
				{
					return false;
				}
			}

			return true;
		}
	};

	struct CameraView
	{
		Matrix4 View;
		Matrix4 Projection;
		Matrix4 ViewProjection;
		Vector3 Position;
		float NearPlane = 0.1f;
		float VerticalFov = Math::ToRadians(60.0f);
		float AspectRatio = 1.0f;
		Frustum Frustum;
	};
}
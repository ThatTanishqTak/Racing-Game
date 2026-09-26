#pragma once

#include "Powertrain/Math/MathUtilities.hpp"
#include "Powertrain/Math/Matrix4.hpp"
#include "Powertrain/Math/Vector3.hpp"

#include <limits>

namespace Powertrain
{
	struct BoundingBox
	{
		Vector3 Min;
		Vector3 Max;

		static constexpr BoundingBox Empty()
		{
			constexpr float l_Big = std::numeric_limits<float>::max();

			return { { l_Big, l_Big, l_Big }, { -l_Big, -l_Big, -l_Big } };
		}

		constexpr bool IsValid() const { return Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z; }

		constexpr void Encapsulate(const Vector3& point)
		{
			Min = Vector3::Min(Min, point);
			Max = Vector3::Max(Max, point);
		}

		constexpr Vector3 GetCenter() const { return (Min + Max) * 0.5f; }
		constexpr Vector3 GetExtents() const { return (Max - Min) * 0.5f; }

		constexpr BoundingBox Transformed(const Matrix4& transform) const
		{
			const Vector3 l_Extents = GetExtents();
			const Vector3 l_Center = transform.TransformPoint(GetCenter());

			const Vector3 l_WorldExtents =
			{
				Math::Abs(transform.M[0][0]) * l_Extents.X + Math::Abs(transform.M[1][0]) * l_Extents.Y + Math::Abs(transform.M[2][0]) * l_Extents.Z,
				Math::Abs(transform.M[0][1]) * l_Extents.X + Math::Abs(transform.M[1][1]) * l_Extents.Y + Math::Abs(transform.M[2][1]) * l_Extents.Z,
				Math::Abs(transform.M[0][2]) * l_Extents.X + Math::Abs(transform.M[1][2]) * l_Extents.Y + Math::Abs(transform.M[2][2]) * l_Extents.Z
			};

			return { l_Center - l_WorldExtents, l_Center + l_WorldExtents };
		}

		constexpr bool operator==(const BoundingBox&) const = default;
	};
}
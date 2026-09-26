#pragma once

#include "Powertrain/Math/Vector3.hpp"
#include "Powertrain/Renderer/RenderTypes.hpp"

#include <cstdint>

namespace Powertrain
{
	namespace MeshPrimitives
	{
		MeshData MakeBox(const Vector3& halfExtents);
		MeshData MakeSphere(float radius, uint32_t segments = 32, uint32_t rings = 16);
		MeshData MakePlane(float halfWidth, float halfDepth);
	}
}
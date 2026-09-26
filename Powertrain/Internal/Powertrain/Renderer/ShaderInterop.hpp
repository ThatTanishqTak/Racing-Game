#pragma once

#include "Powertrain/Math/Matrix4.hpp"
#include "Powertrain/Math/Vector2.hpp"
#include "Powertrain/Math/Vector3.hpp"
#include "Powertrain/Math/Vector4.hpp"
#include "Powertrain/Renderer/RenderTypes.hpp"

#include <cstdint>

namespace Powertrain
{
	namespace ShaderInterop
	{
		// The HLSL scalar and vector names, so ShaderInterop.hlsli compiles as C++ with the engine's math types underneath
		using uint = uint32_t;
		using float2 = Vector2;
		using float3 = Vector3;
		using float4 = Vector4;
		using float4x4 = Matrix4;

		struct uint4
		{
			uint X = 0;
			uint Y = 0;
			uint Z = 0;
			uint W = 0;
		};

		// Matrix4 already stores rows, so the HLSL qualifier has nothing to do on this side
#define row_major
#include "ShaderInterop.hlsli"
#undef row_major

		static_assert(sizeof(Vertex) == k_VertexStride, "Vertex must match the stride Forward.hlsl pulls with");
		static_assert(sizeof(DrawConstants) == 8 * sizeof(uint32_t), "DrawConstants must be exactly 8 root constants");
		static_assert(sizeof(InstanceData) == 64, "InstanceData must be one matrix so the ring offset divides into whole instances");
		static_assert(sizeof(MaterialData) == 64, "MaterialData must stay 64 bytes so the ring offset divides into whole materials");
		static_assert(sizeof(FrameConstants) % 16 == 0, "FrameConstants must fill whole 16-byte constant registers");
	}
}
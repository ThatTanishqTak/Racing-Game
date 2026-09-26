#include "Powertrain/Renderer/MeshPrimitives.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Math/MathUtilities.hpp"

#include <cmath>

namespace Powertrain
{
	namespace
	{
		float ExtentAlong(const Vector3& halfExtents, const Vector3& axis)
		{
			return Math::Abs(Vector3::Dot(halfExtents, axis));
		}

		void AppendQuad(MeshData& mesh, const Vector3& center, const Vector3& normal, const Vector3& tangent, float halfAlongTangent, float halfAlongBitangent)
		{
			const Vector3 l_Bitangent = Vector3::Cross(normal, tangent);
			const uint32_t l_Base = static_cast<uint32_t>(mesh.Vertices.size());

			const Vector3 l_Corners[4] =
			{
				center - tangent * halfAlongTangent - l_Bitangent * halfAlongBitangent,
				center + tangent * halfAlongTangent - l_Bitangent * halfAlongBitangent,
				center + tangent * halfAlongTangent + l_Bitangent * halfAlongBitangent,
				center - tangent * halfAlongTangent + l_Bitangent * halfAlongBitangent
			};
			const Vector2 l_TexCoords[4] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

			for (uint32_t l_Index = 0; l_Index < 4; ++l_Index)
			{
				mesh.Vertices.push_back({ l_Corners[l_Index], normal, tangent, 1.0f, l_TexCoords[l_Index] });
			}

			mesh.Indices.insert(mesh.Indices.end(), { l_Base, l_Base + 1, l_Base + 2, l_Base, l_Base + 2, l_Base + 3 });
		}

		void FinishSingleSubmesh(MeshData& mesh)
		{
			mesh.Submeshes.push_back({ 0, static_cast<uint32_t>(mesh.Indices.size()), 0 });
		}
	}

	namespace MeshPrimitives
	{
		MeshData MakeBox(const Vector3& halfExtents)
		{
			PT_CORE_ASSERT(halfExtents.X > 0.0f && halfExtents.Y > 0.0f && halfExtents.Z > 0.0f, "Box half extents must be positive, got ({}, {}, {})", halfExtents.X, halfExtents.Y, halfExtents.Z);

			const Vector3 l_Faces[6][2] =
			{
				{ Vector3::UnitX(), -Vector3::UnitZ() },
				{ -Vector3::UnitX(), Vector3::UnitZ() },
				{ Vector3::UnitY(), Vector3::UnitX() },
				{ -Vector3::UnitY(), Vector3::UnitX() },
				{ Vector3::UnitZ(), Vector3::UnitX() },
				{ -Vector3::UnitZ(), -Vector3::UnitX() }
			};

			MeshData l_Mesh;
			l_Mesh.Vertices.reserve(24);
			l_Mesh.Indices.reserve(36);

			for (const auto& l_Face : l_Faces)
			{
				const Vector3& l_Normal = l_Face[0];
				const Vector3& l_Tangent = l_Face[1];
				const Vector3 l_Bitangent = Vector3::Cross(l_Normal, l_Tangent);

				AppendQuad(l_Mesh, l_Normal * ExtentAlong(halfExtents, l_Normal), l_Normal, l_Tangent, ExtentAlong(halfExtents, l_Tangent), ExtentAlong(halfExtents, l_Bitangent));
			}

			FinishSingleSubmesh(l_Mesh);

			return l_Mesh;
		}

		MeshData MakeSphere(float radius, uint32_t segments, uint32_t rings)
		{
			PT_CORE_ASSERT(radius > 0.0f, "Sphere radius must be positive, got {}", radius);
			PT_CORE_ASSERT(segments >= 3 && rings >= 2, "Sphere needs at least 3 segments and 2 rings, got {} and {}", segments, rings);

			MeshData l_Mesh;
			l_Mesh.Vertices.reserve(static_cast<size_t>(rings + 1) * (segments + 1));
			l_Mesh.Indices.reserve(static_cast<size_t>(rings) * segments * 6);

			for (uint32_t l_Ring = 0; l_Ring <= rings; ++l_Ring)
			{
				const float l_Theta = Math::k_Pi * static_cast<float>(l_Ring) / static_cast<float>(rings);
				const float l_SinTheta = std::sin(l_Theta);
				const float l_CosTheta = std::cos(l_Theta);

				for (uint32_t l_Segment = 0; l_Segment <= segments; ++l_Segment)
				{
					const float l_Phi = Math::k_TwoPi * static_cast<float>(l_Segment) / static_cast<float>(segments);
					const float l_SinPhi = std::sin(l_Phi);
					const float l_CosPhi = std::cos(l_Phi);

					const Vector3 l_Normal = { l_SinTheta * l_CosPhi, l_CosTheta, l_SinTheta * l_SinPhi };
					const Vector3 l_Tangent = { -l_SinPhi, 0.0f, l_CosPhi };
					const Vector2 l_TexCoord = { static_cast<float>(l_Segment) / static_cast<float>(segments), static_cast<float>(l_Ring) / static_cast<float>(rings) };

					l_Mesh.Vertices.push_back({ l_Normal * radius, l_Normal, l_Tangent, 1.0f, l_TexCoord });
				}
			}

			const uint32_t l_Stride = segments + 1;
			for (uint32_t l_Ring = 0; l_Ring < rings; ++l_Ring)
			{
				for (uint32_t l_Segment = 0; l_Segment < segments; ++l_Segment)
				{
					const uint32_t l_A = l_Ring * l_Stride + l_Segment;
					const uint32_t l_B = l_A + 1;
					const uint32_t l_C = l_A + l_Stride;
					const uint32_t l_D = l_C + 1;

					l_Mesh.Indices.insert(l_Mesh.Indices.end(), { l_A, l_B, l_D, l_A, l_D, l_C });
				}
			}

			FinishSingleSubmesh(l_Mesh);

			return l_Mesh;
		}

		MeshData MakePlane(float halfWidth, float halfDepth)
		{
			PT_CORE_ASSERT(halfWidth > 0.0f && halfDepth > 0.0f, "Plane half sizes must be positive, got {} and {}", halfWidth, halfDepth);

			MeshData l_Mesh;
			l_Mesh.Vertices.reserve(4);
			l_Mesh.Indices.reserve(6);

			AppendQuad(l_Mesh, Vector3::Zero(), Vector3::UnitY(), Vector3::UnitX(), halfWidth, halfDepth);
			FinishSingleSubmesh(l_Mesh);

			return l_Mesh;
		}
	}
}
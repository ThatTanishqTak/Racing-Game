#pragma once

#include "Powertrain/Core/Handle.hpp"
#include "Powertrain/Math/Vector2.hpp"
#include "Powertrain/Math/Vector3.hpp"

#include <cstdint>
#include <vector>

namespace Powertrain
{
	// Linear RGBA in [0, 1]; the renderer encodes to sRGB on output
	struct Color
	{
		float R = 1.0f;
		float G = 1.0f;
		float B = 1.0f;
		float A = 1.0f;
	};

	struct Vertex
	{
		Vector3 Position;
		Vector3 Normal;
		Vector3 Tangent;
		float TangentSign = 1.0f;
		Vector2 TexCoord;
	};

	struct Submesh
	{
		uint32_t IndexOffset = 0;
		uint32_t IndexCount = 0;
		uint32_t MaterialIndex = 0;
	};

	struct MeshData
	{
		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;
		std::vector<Submesh> Submeshes;
	};

	// Formats the M7 loaders produce; block-compressed ones arrive with the DDS loader
	enum class TextureFormat : uint8_t
	{
		RGBA8Unorm,
		RGBA8Srgb,
		RGBA16Float,
		BC7Unorm,
		BC7Srgb
	};

	struct TextureData
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t MipCount = 1;
		TextureFormat Format = TextureFormat::RGBA8Srgb;

		// Mips packed tightly, largest first
		std::vector<uint8_t> Pixels;
	};

	enum class AlphaMode : uint8_t
	{
		Opaque,
		Mask,
		Blend
	};

	struct MaterialDescription
	{
		Color BaseColor;
		float Metallic = 0.0f;
		float Roughness = 1.0f;
		Color Emissive = { 0.0f, 0.0f, 0.0f, 1.0f };
		float AlphaCutoff = 0.5f;
		AlphaMode Alpha = AlphaMode::Opaque;

		TextureHandle BaseColorTexture;
		TextureHandle MetallicRoughnessTexture;
		TextureHandle NormalTexture;
		TextureHandle EmissiveTexture;
	};

	struct EnvironmentSettings
	{
		Vector3 SunDirection = { -0.3f, -1.0f, -0.2f };
		Color SunColor;
		float SunIntensity = 10.0f;
		Color SkyTint = { 0.5f, 0.7f, 1.0f, 1.0f };
		float Exposure = 1.0f;
	};

	struct RendererStats
	{
		double CpuFrameMilliseconds = 0.0;
		double GpuFrameMilliseconds = 0.0;
		uint32_t DrawCalls = 0;
		uint32_t Triangles = 0;
		uint32_t ShadowDrawCalls = 0;
		uint32_t Instances = 0;
		uint32_t CulledInstances = 0;
		uint32_t Meshes = 0;
		uint32_t Textures = 0;
		uint32_t Materials = 0;
		uint64_t GpuMemoryBytes = 0;
		uint64_t FrameIndex = 0;
	};
}
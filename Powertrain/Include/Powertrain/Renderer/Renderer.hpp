#pragma once

#include "Powertrain/Core/Handle.hpp"
#include "Powertrain/Math/Matrix4.hpp"
#include "Powertrain/Math/Vector3.hpp"
#include "Powertrain/Renderer/RenderTypes.hpp"

#include <string_view>

namespace Powertrain
{
	struct RendererSettings
	{
		bool VSync = true;
		Color ClearColor = { 0.10f, 0.10f, 0.12f, 1.0f };

#ifdef PT_DEBUG
		bool EnableDebugLayer = true;
#else
		bool EnableDebugLayer = false;
#endif
		bool EnableGpuBasedValidation = false;
	};

	class DebugDraw
	{
	public:
		virtual ~DebugDraw() = default;

		virtual void Line(const Vector3& from, const Vector3& to, const Color& color) = 0;
		virtual void Arrow(const Vector3& origin, const Vector3& vector, const Color& color) = 0;
		virtual void Box(const Matrix4& transform, const Vector3& halfExtents, const Color& color) = 0;
		virtual void Sphere(const Vector3& center, float radius, const Color& color) = 0;
	};

	class Renderer
	{
	public:
		virtual ~Renderer() = default;

		// Resources
		virtual MeshHandle CreateMesh(const MeshData& data) = 0;
		virtual void DestroyMesh(MeshHandle mesh) = 0;
		virtual TextureHandle CreateTexture(const TextureData& data) = 0;
		virtual void DestroyTexture(TextureHandle texture) = 0;
		virtual MaterialHandle CreateMaterial(const MaterialDescription& description) = 0;
		virtual void UpdateMaterial(MaterialHandle material, const MaterialDescription& description) = 0;

		// Presentation
		virtual void SetVSync(bool enabled) = 0;
		virtual bool IsVSyncEnabled() const = 0;
		virtual void SetClearColor(const Color& color) = 0;
		virtual void SetEnvironment(const EnvironmentSettings& environment) = 0;

		// Tooling
		virtual DebugDraw& GetDebugDraw() = 0;
		virtual const RendererStats& GetStats() const = 0;
		virtual std::string_view GetAdapterName() const = 0;
	};
}
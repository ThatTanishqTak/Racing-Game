#pragma once

#include "Powertrain/Renderer/Renderer.hpp"
#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <cstdint>
#include <vector>

namespace Powertrain
{
	class D3D12Renderer;

	class DebugDrawPass final : public DebugDraw
	{
	public:
		static constexpr uint32_t k_MaxVertices = 131072;

		DebugDrawPass() = default;
		~DebugDrawPass() override;

		DebugDrawPass(const DebugDrawPass&) = delete;
		DebugDrawPass& operator=(const DebugDrawPass&) = delete;

		bool Initialize(D3D12Renderer& renderer);
		void Shutdown();

		void Render(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS frameConstants);

		// DebugDraw
		void Line(const Vector3& from, const Vector3& to, const Color& color) override;
		void Arrow(const Vector3& origin, const Vector3& vector, const Color& color) override;
		void Box(const Matrix4& transform, const Vector3& halfExtents, const Color& color) override;
		void Sphere(const Vector3& center, float radius, const Color& color) override;

		uint32_t GetLineCount() const { return static_cast<uint32_t>(m_Vertices.size() / 2); }
		uint32_t GetDrawCallCount() const { return m_DrawCalls; }

	private:
		// Matches LineVertex in DebugLine.hlsl
		struct LineVertex
		{
			float Position[3];
			uint32_t Color;
		};

		void Push(const Vector3& from, const Vector3& to, uint32_t color);

	private:
		D3D12Renderer* m_Renderer = nullptr;
		ID3D12RootSignature* m_RootSignature = nullptr;
		ID3D12PipelineState* m_Pipeline = nullptr;

		std::vector<LineVertex> m_Vertices;
		uint32_t m_DroppedVertices = 0;
		uint32_t m_DrawCalls = 0;

		bool m_Initialized = false;
		bool m_OverflowWarned = false;
	};
}
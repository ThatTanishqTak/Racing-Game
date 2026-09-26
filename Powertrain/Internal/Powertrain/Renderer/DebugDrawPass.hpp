#pragma once

#include "Powertrain/Renderer/Renderer.hpp"
#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace Powertrain
{
	class D3D12Renderer;

	// Pass 7: the line batcher behind Renderer::GetDebugDraw(). Lines collected anywhere in the frame are drawn once,
	// after the layers' OnRender and before ImGui, then dropped. Until M6 they go straight into the swap chain with no depth test.
	class DebugDrawPass final : public DebugDraw
	{
	public:
		static constexpr uint32_t k_MaxVertices = 131072;

		DebugDrawPass() = default;
		~DebugDrawPass() override;

		DebugDrawPass(const DebugDrawPass&) = delete;
		DebugDrawPass& operator=(const DebugDrawPass&) = delete;

		bool Initialize(D3D12Renderer& renderer);

		// Call after the direct queue has been flushed; the vertex buffers are unmapped and released here
		void Shutdown();

		// Uploads this frame's lines into the frame's buffer, draws them with the view-projection, then clears the batch
		void Render(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex, const Matrix4& viewProjection);

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

		struct FrameBuffer
		{
			ComPtr<ID3D12Resource> Buffer;
			LineVertex* Mapped = nullptr;
		};

		bool CreateRootSignature(ID3D12Device* device);
		bool CreatePipelineState(ID3D12Device* device, const std::vector<uint8_t>& vertexShader, const std::vector<uint8_t>& pixelShader);
		bool CreateVertexBuffers(ID3D12Device* device);
		void Push(const Vector3& from, const Vector3& to, uint32_t color);

	private:
		ComPtr<ID3D12RootSignature> m_RootSignature;
		ComPtr<ID3D12PipelineState> m_PipelineState;
		std::array<FrameBuffer, D3D12::k_FramesInFlight> m_VertexBuffers;

		std::vector<LineVertex> m_Vertices;
		uint32_t m_DroppedVertices = 0;
		uint32_t m_DrawCalls = 0;

		bool m_Initialized = false;
		bool m_OverflowWarned = false;
	};
}
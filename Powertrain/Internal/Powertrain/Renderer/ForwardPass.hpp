#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"
#include "Powertrain/RHI/D3D12/D3D12DescriptorHeap.hpp"

#include <cstdint>

namespace Powertrain
{
	class D3D12Renderer;

	class ForwardPass
	{
	public:
		ForwardPass() = default;
		~ForwardPass();

		ForwardPass(const ForwardPass&) = delete;
		ForwardPass& operator=(const ForwardPass&) = delete;

		bool Initialize(D3D12Renderer& renderer);

		// Call after the direct queue has been flushed; the vertex buffer and its descriptor are released here
		void Shutdown();

		// Binds the global root signature and the frame constants, then draws; the swap chain RTV is already bound
		void Render(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS frameConstants);

		uint32_t GetDrawCallCount() const { return m_DrawCalls; }
		uint32_t GetTriangleCount() const { return m_Triangles; }

	private:
		bool CreateTriangle(D3D12Renderer& renderer);

	private:
		D3D12DescriptorHeap* m_ResourceHeap = nullptr;
		ID3D12RootSignature* m_RootSignature = nullptr;
		ID3D12PipelineState* m_Pipeline = nullptr;

		ComPtr<ID3D12Resource> m_TriangleVertices;
		DescriptorHandle m_TriangleView;
		uint32_t m_TriangleVertexCount = 0;

		uint32_t m_DrawCalls = 0;
		uint32_t m_Triangles = 0;

		bool m_Initialized = false;
	};
}
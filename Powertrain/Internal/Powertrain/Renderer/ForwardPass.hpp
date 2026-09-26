#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"
#include "Powertrain/RHI/D3D12/D3D12MeshStorage.hpp"

#include <cstdint>
#include <span>

namespace Powertrain
{
	class D3D12Renderer;

	struct DrawBatch
	{
		const D3D12Mesh* Mesh = nullptr;
		uint32_t MaterialIndex = 0;
		uint32_t FirstInstance = 0;
		uint32_t InstanceCount = 0;
	};

	class ForwardPass
	{
	public:
		ForwardPass() = default;
		~ForwardPass();

		ForwardPass(const ForwardPass&) = delete;
		ForwardPass& operator=(const ForwardPass&) = delete;

		bool Initialize(D3D12Renderer& renderer);
		void Shutdown();
		void Render(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS frameConstants, std::span<const DrawBatch> batches);

		uint32_t GetDrawCallCount() const { return m_DrawCalls; }
		uint32_t GetTriangleCount() const { return m_Triangles; }

	private:
		ID3D12RootSignature* m_RootSignature = nullptr;
		ID3D12PipelineState* m_Pipeline = nullptr;

		uint32_t m_DrawCalls = 0;
		uint32_t m_Triangles = 0;

		bool m_Initialized = false;
	};
}
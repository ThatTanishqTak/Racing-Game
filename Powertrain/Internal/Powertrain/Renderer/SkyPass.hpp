#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <cstdint>

namespace Powertrain
{
	class D3D12Renderer;

	// Pass 5: the analytic sky as one fullscreen triangle at depth 0, so under reversed-Z it fills only what the opaque pass left empty
	class SkyPass
	{
	public:
		SkyPass() = default;
		~SkyPass();

		SkyPass(const SkyPass&) = delete;
		SkyPass& operator=(const SkyPass&) = delete;

		bool Initialize(D3D12Renderer& renderer);
		void Shutdown();

		void Render(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS frameConstants);

		uint32_t GetDrawCallCount() const { return m_DrawCalls; }

	private:
		ID3D12RootSignature* m_RootSignature = nullptr;
		ID3D12PipelineState* m_Pipeline = nullptr;

		uint32_t m_DrawCalls = 0;

		bool m_Initialized = false;
	};
}
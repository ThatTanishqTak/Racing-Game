#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <cstdint>

namespace Powertrain
{
	class D3D12Renderer;
	class D3D12SceneTarget;
	class D3D12UploadRing;

	class PostPass
	{
	public:
		PostPass() = default;
		~PostPass();

		PostPass(const PostPass&) = delete;
		PostPass& operator=(const PostPass&) = delete;

		bool Initialize(D3D12Renderer& renderer);
		void Shutdown();

		// Leaves the swap chain view bound without depth, which is the state the ImGui pass wants
		void Render(ID3D12GraphicsCommandList* commandList, D3D12SceneTarget& sceneTarget, D3D12UploadRing& uploadRing, D3D12_CPU_DESCRIPTOR_HANDLE swapChainRtv, float exposure);

		uint32_t GetDrawCallCount() const { return m_DrawCalls; }

	private:
		ID3D12RootSignature* m_RootSignature = nullptr;
		ID3D12PipelineState* m_Pipeline = nullptr;

		uint32_t m_DrawCalls = 0;

		bool m_Initialized = false;
	};
}
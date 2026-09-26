#pragma once

#include "Powertrain/Math/Matrix4.hpp"
#include "Powertrain/Renderer/Camera.hpp"
#include "Powertrain/Renderer/ForwardPass.hpp"
#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace Powertrain
{
	class D3D12Renderer;
	class D3D12ShadowMap;
	class D3D12UploadRing;

	struct ShadowCascade
	{
		Matrix4 ViewProjection;
		float SplitDistance = 0.0f;
		float TexelWorldSize = 0.0f;
		Frustum Frustum;
		std::vector<DrawBatch> Batches;
	};

	class ShadowPass
	{
	public:
		ShadowPass() = default;
		~ShadowPass();

		ShadowPass(const ShadowPass&) = delete;
		ShadowPass& operator=(const ShadowPass&) = delete;

		bool Initialize(D3D12Renderer& renderer);
		void Shutdown();

		void Render(ID3D12GraphicsCommandList* commandList, D3D12ShadowMap& shadowMap, D3D12UploadRing& uploadRing, D3D12_GPU_VIRTUAL_ADDRESS frameConstants, std::span<const ShadowCascade> cascades);

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
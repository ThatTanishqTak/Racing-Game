#pragma once

#include "Powertrain/Math/Vector3.hpp"
#include "Powertrain/Renderer/RenderTypes.hpp"
#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <cstdint>

namespace Powertrain
{
	class D3D12EnvironmentMaps;
	class D3D12Renderer;
	class D3D12UploadRing;
	struct SunLight;

	class EnvironmentPass
	{
	public:
		EnvironmentPass() = default;
		~EnvironmentPass();

		EnvironmentPass(const EnvironmentPass&) = delete;
		EnvironmentPass& operator=(const EnvironmentPass&) = delete;

		bool Initialize(D3D12Renderer& renderer);
		void Shutdown();

		bool NeedsBake(const SunLight& sun, const Color& skyTint) const;
		void Render(ID3D12GraphicsCommandList* commandList, D3D12EnvironmentMaps& maps, D3D12UploadRing& uploadRing, D3D12_GPU_VIRTUAL_ADDRESS frameConstants, const SunLight& sun, const Color& skyTint);

		uint32_t GetDrawCallCount() const { return m_DrawCalls; }
		uint32_t GetBakeCount() const { return m_BakeCount; }

	private:
		bool DrawFace(ID3D12GraphicsCommandList* commandList, D3D12UploadRing& uploadRing, D3D12_CPU_DESCRIPTOR_HANDLE target, uint32_t size, uint32_t face, uint32_t sampleCount, float roughness);

	private:
		ID3D12RootSignature* m_RootSignature = nullptr;
		ID3D12PipelineState* m_SpecularPipeline = nullptr;
		ID3D12PipelineState* m_IrradiancePipeline = nullptr;
		ID3D12PipelineState* m_BrdfPipeline = nullptr;

		Vector3 m_BakedTowardsSun;
		Vector3 m_BakedRadiance;
		Color m_BakedTint;

		uint32_t m_DrawCalls = 0;
		uint32_t m_BakeCount = 0;

		bool m_Baked = false;
		bool m_BrdfBaked = false;
		bool m_Initialized = false;
	};
}
#include "Powertrain/Renderer/SkyPass.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12DepthBuffer.hpp"
#include "Powertrain/RHI/D3D12/D3D12PipelineCache.hpp"
#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"
#include "Powertrain/RHI/D3D12/D3D12SceneTarget.hpp"

namespace Powertrain
{
	SkyPass::~SkyPass()
	{
		Shutdown();
	}

	bool SkyPass::Initialize(D3D12Renderer& renderer)
	{
		D3D12PipelineCache& l_Cache = renderer.GetPipelineCache();
		m_RootSignature = l_Cache.GetRootSignature();

		// Depth-tested GREATER_EQUAL at depth 0 without writing: the cleared background passes, anything drawn in front does not
		GraphicsPipelineDescription l_Description;
		l_Description.VertexShader = "Sky.VSMain";
		l_Description.PixelShader = "Sky.PSMain";
		l_Description.RenderTargetFormats[0] = D3D12SceneTarget::k_Format;
		l_Description.RenderTargetCount = 1;
		l_Description.DepthFormat = D3D12DepthBuffer::k_Format;
		l_Description.SampleCount = renderer.GetSceneTarget().GetSampleCount();
		l_Description.DepthTest = true;
		l_Description.DepthWrite = false;
		l_Description.CullMode = D3D12_CULL_MODE_NONE;

		m_Pipeline = l_Cache.GetGraphicsPipeline(l_Description);
		if (m_Pipeline == nullptr)
		{
			Shutdown();

			return false;
		}

		m_DrawCalls = 0;
		m_Initialized = true;

		PT_CORE_INFO("Sky pass ready");

		return true;
	}

	void SkyPass::Shutdown()
	{
		m_Pipeline = nullptr;
		m_RootSignature = nullptr;
		m_DrawCalls = 0;

		if (m_Initialized)
		{
			PT_CORE_INFO("Sky pass shut down");
		}

		m_Initialized = false;
	}

	void SkyPass::Render(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS frameConstants)
	{
		m_DrawCalls = 0;

		if (!m_Initialized || frameConstants == 0)
		{
			return;
		}

		commandList->SetGraphicsRootSignature(m_RootSignature);
		commandList->SetPipelineState(m_Pipeline);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->SetGraphicsRootConstantBufferView(D3D12PipelineCache::k_FrameConstantsParameter, frameConstants);
		commandList->DrawInstanced(3, 1, 0, 0);

		m_DrawCalls = 1;
	}
}
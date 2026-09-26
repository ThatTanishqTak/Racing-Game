#include "Powertrain/Renderer/PostPass.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/RHI/D3D12/D3D12PipelineCache.hpp"
#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"
#include "Powertrain/RHI/D3D12/D3D12SceneTarget.hpp"
#include "Powertrain/RHI/D3D12/D3D12SwapChain.hpp"
#include "Powertrain/RHI/D3D12/D3D12UploadRing.hpp"

namespace Powertrain
{
	PostPass::~PostPass()
	{
		Shutdown();
	}

	bool PostPass::Initialize(D3D12Renderer& renderer)
	{
		D3D12PipelineCache& l_Cache = renderer.GetPipelineCache();
		m_RootSignature = l_Cache.GetRootSignature();

		// A fullscreen triangle into the sRGB swap chain, no depth, no culling; the target encodes on write
		GraphicsPipelineDescription l_Description;
		l_Description.VertexShader = "Post.VSMain";
		l_Description.PixelShader = "Post.PSMain";
		l_Description.RenderTargetFormats[0] = D3D12SwapChain::k_RtvFormat;
		l_Description.RenderTargetCount = 1;
		l_Description.CullMode = D3D12_CULL_MODE_NONE;

		m_Pipeline = l_Cache.GetGraphicsPipeline(l_Description);
		if (m_Pipeline == nullptr)
		{
			Shutdown();

			return false;
		}

		m_DrawCalls = 0;
		m_Initialized = true;

		PT_CORE_INFO("Post pass ready");

		return true;
	}

	void PostPass::Shutdown()
	{
		m_Pipeline = nullptr;
		m_RootSignature = nullptr;
		m_DrawCalls = 0;

		if (m_Initialized)
		{
			PT_CORE_INFO("Post pass shut down");
		}

		m_Initialized = false;
	}

	void PostPass::Render(ID3D12GraphicsCommandList* commandList, D3D12SceneTarget& sceneTarget, D3D12UploadRing& uploadRing, D3D12_CPU_DESCRIPTOR_HANDLE swapChainRtv, float exposure)
	{
		m_DrawCalls = 0;

		if (!m_Initialized)
		{
			return;
		}

		sceneTarget.Resolve(commandList);

		commandList->OMSetRenderTargets(1, &swapChainRtv, FALSE, nullptr);

		ShaderInterop::PostPassConstants l_Constants = {};
		l_Constants.Params = { exposure, 0.0f, 0.0f, 0.0f };

		// Without its constants the pass leaves the swap chain as it was rather than tonemapping with garbage
		const UploadAllocation l_Allocation = uploadRing.Upload(l_Constants);
		if (!l_Allocation.IsValid())
		{
			return;
		}

		// The resolved texture rides in the pass data slot of the root constants; nothing else in DrawConstants applies here
		ShaderInterop::DrawConstants l_Draw = {};
		l_Draw.PassDataIndex = sceneTarget.GetResolvedIndex();

		commandList->SetGraphicsRootSignature(m_RootSignature);
		commandList->SetPipelineState(m_Pipeline);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->SetGraphicsRoot32BitConstants(D3D12PipelineCache::k_DrawConstantsParameter, D3D12PipelineCache::k_DrawConstantCount, &l_Draw, 0);
		commandList->SetGraphicsRootConstantBufferView(D3D12PipelineCache::k_PassConstantsParameter, l_Allocation.Gpu);
		commandList->DrawInstanced(3, 1, 0, 0);

		m_DrawCalls = 1;
	}
}
#include "Powertrain/Renderer/ForwardPass.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/RHI/D3D12/D3D12DepthBuffer.hpp"
#include "Powertrain/RHI/D3D12/D3D12PipelineCache.hpp"
#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"
#include "Powertrain/RHI/D3D12/D3D12SceneTarget.hpp"

namespace Powertrain
{
	ForwardPass::~ForwardPass()
	{
		Shutdown();
	}

	bool ForwardPass::Initialize(D3D12Renderer& renderer)
	{
		D3D12PipelineCache& l_Cache = renderer.GetPipelineCache();
		m_RootSignature = l_Cache.GetRootSignature();

		// Opaque, back-face culled with counter-clockwise fronts, reversed-Z depth test and write into the multisampled HDR scene target
		GraphicsPipelineDescription l_Description;
		l_Description.VertexShader = "Forward.VSMain";
		l_Description.PixelShader = "Forward.PSMain";
		l_Description.RenderTargetFormats[0] = D3D12SceneTarget::k_Format;
		l_Description.RenderTargetCount = 1;
		l_Description.DepthFormat = D3D12DepthBuffer::k_Format;
		l_Description.SampleCount = renderer.GetSceneTarget().GetSampleCount();
		l_Description.DepthTest = true;
		l_Description.DepthWrite = true;

		m_Pipeline = l_Cache.GetGraphicsPipeline(l_Description);
		if (m_Pipeline == nullptr)
		{
			Shutdown();

			return false;
		}

		m_DrawCalls = 0;
		m_Triangles = 0;
		m_Initialized = true;

		PT_CORE_INFO("Forward pass ready");

		return true;
	}

	void ForwardPass::Shutdown()
	{
		m_Pipeline = nullptr;
		m_RootSignature = nullptr;
		m_DrawCalls = 0;
		m_Triangles = 0;

		if (m_Initialized)
		{
			PT_CORE_INFO("Forward pass shut down");
		}

		m_Initialized = false;
	}

	void ForwardPass::Render(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS frameConstants, std::span<const DrawBatch> batches)
	{
		m_DrawCalls = 0;
		m_Triangles = 0;

		if (!m_Initialized || frameConstants == 0 || batches.empty())
		{
			return;
		}

		commandList->SetGraphicsRootSignature(m_RootSignature);
		commandList->SetPipelineState(m_Pipeline);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->SetGraphicsRootConstantBufferView(D3D12PipelineCache::k_FrameConstantsParameter, frameConstants);

		for (const DrawBatch& l_Batch : batches)
		{
			const D3D12Mesh& l_Mesh = *l_Batch.Mesh;

			// The shader adds SV_InstanceID to InstanceIndex, so StartInstanceLocation stays 0 and the instance buffer index lives in the frame constants
			ShaderInterop::DrawConstants l_Draw = {};
			l_Draw.InstanceIndex = l_Batch.FirstInstance;
			l_Draw.MaterialIndex = l_Batch.MaterialIndex;
			l_Draw.VertexBufferIndex = l_Mesh.VertexView.Index;
			commandList->SetGraphicsRoot32BitConstants(D3D12PipelineCache::k_DrawConstantsParameter, D3D12PipelineCache::k_DrawConstantCount, &l_Draw, 0);
			commandList->IASetIndexBuffer(&l_Mesh.IndexBufferView);

			for (const Submesh& l_Submesh : l_Mesh.Submeshes)
			{
				commandList->DrawIndexedInstanced(l_Submesh.IndexCount, l_Batch.InstanceCount, l_Submesh.IndexOffset, 0, 0);

				++m_DrawCalls;
				m_Triangles += (l_Submesh.IndexCount / 3) * l_Batch.InstanceCount;
			}
		}
	}
}
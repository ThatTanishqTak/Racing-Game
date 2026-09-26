#include "Powertrain/Renderer/ShadowPass.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/RHI/D3D12/D3D12PipelineCache.hpp"
#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"
#include "Powertrain/RHI/D3D12/D3D12ShadowMap.hpp"
#include "Powertrain/RHI/D3D12/D3D12UploadRing.hpp"

namespace Powertrain
{
	namespace
	{
		// Reversed-Z, so pushing depth away from the light means a negative bias; the receiver-side normal offset in the
		// forward shader does most of the work, this only settles the last bit of acne on grazing faces
		constexpr int32_t k_DepthBias = -64;
		constexpr float k_SlopeScaledDepthBias = -1.5f;
	}

	ShadowPass::~ShadowPass()
	{
		Shutdown();
	}

	bool ShadowPass::Initialize(D3D12Renderer& renderer)
	{
		D3D12PipelineCache& l_Cache = renderer.GetPipelineCache();
		m_RootSignature = l_Cache.GetRootSignature();

		// Depth only, no colour target and no pixel shader. Culling is off so one-sided geometry such as the ground plane
		// still casts, and alpha-masked materials cast their full silhouette until the pass gains a masked variant
		GraphicsPipelineDescription l_Description;
		l_Description.VertexShader = "Shadow.VSMain";
		l_Description.RenderTargetCount = 0;
		l_Description.DepthFormat = D3D12ShadowMap::k_DepthFormat;
		l_Description.DepthTest = true;
		l_Description.DepthWrite = true;
		l_Description.CullMode = D3D12_CULL_MODE_NONE;
		l_Description.DepthBias = k_DepthBias;
		l_Description.SlopeScaledDepthBias = k_SlopeScaledDepthBias;

		m_Pipeline = l_Cache.GetGraphicsPipeline(l_Description);
		if (m_Pipeline == nullptr)
		{
			Shutdown();

			return false;
		}

		m_DrawCalls = 0;
		m_Triangles = 0;
		m_Initialized = true;

		PT_CORE_INFO("Shadow pass ready");

		return true;
	}

	void ShadowPass::Shutdown()
	{
		m_Pipeline = nullptr;
		m_RootSignature = nullptr;
		m_DrawCalls = 0;
		m_Triangles = 0;

		if (m_Initialized)
		{
			PT_CORE_INFO("Shadow pass shut down");
		}

		m_Initialized = false;
	}

	void ShadowPass::Render(ID3D12GraphicsCommandList* commandList, D3D12ShadowMap& shadowMap, D3D12UploadRing& uploadRing, D3D12_GPU_VIRTUAL_ADDRESS frameConstants, std::span<const ShadowCascade> cascades)
	{
		m_DrawCalls = 0;
		m_Triangles = 0;

		if (!m_Initialized || frameConstants == 0)
		{
			return;
		}

		shadowMap.TransitionTo(commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		if (!cascades.empty())
		{
			D3D12_VIEWPORT l_Viewport = {};
			l_Viewport.Width = static_cast<float>(D3D12ShadowMap::k_Size);
			l_Viewport.Height = static_cast<float>(D3D12ShadowMap::k_Size);
			l_Viewport.MinDepth = 0.0f;
			l_Viewport.MaxDepth = 1.0f;
			commandList->RSSetViewports(1, &l_Viewport);

			const D3D12_RECT l_Scissor = { 0, 0, static_cast<LONG>(D3D12ShadowMap::k_Size), static_cast<LONG>(D3D12ShadowMap::k_Size) };
			commandList->RSSetScissorRects(1, &l_Scissor);

			commandList->SetGraphicsRootSignature(m_RootSignature);
			commandList->SetPipelineState(m_Pipeline);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->SetGraphicsRootConstantBufferView(D3D12PipelineCache::k_FrameConstantsParameter, frameConstants);

			for (uint32_t l_Index = 0; l_Index < cascades.size(); ++l_Index)
			{
				const ShadowCascade& l_Cascade = cascades[l_Index];

				const D3D12_CPU_DESCRIPTOR_HANDLE l_Dsv = shadowMap.GetDsv(l_Index);
				commandList->OMSetRenderTargets(0, nullptr, FALSE, &l_Dsv);
				shadowMap.Clear(commandList, l_Index);

				if (l_Cascade.Batches.empty())
				{
					continue;
				}

				ShaderInterop::ShadowPassConstants l_PassConstants = {};
				l_PassConstants.ViewProjection = l_Cascade.ViewProjection;
				l_PassConstants.Cascade.X = l_Index;

				const UploadAllocation l_Allocation = uploadRing.Upload(l_PassConstants);
				if (!l_Allocation.IsValid())
				{
					continue;
				}

				commandList->SetGraphicsRootConstantBufferView(D3D12PipelineCache::k_PassConstantsParameter, l_Allocation.Gpu);

				for (const DrawBatch& l_Batch : l_Cascade.Batches)
				{
					const D3D12Mesh& l_Mesh = *l_Batch.Mesh;

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

		shadowMap.TransitionTo(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
}
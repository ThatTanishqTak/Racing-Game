#include "Powertrain/Renderer/EnvironmentPass.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Renderer/SceneRenderer.hpp"
#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/RHI/D3D12/D3D12EnvironmentMaps.hpp"
#include "Powertrain/RHI/D3D12/D3D12PipelineCache.hpp"
#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"
#include "Powertrain/RHI/D3D12/D3D12UploadRing.hpp"

namespace Powertrain
{
	namespace
	{
		// Mip 0 is the sky itself, so one sample; the rougher mips and the irradiance cube integrate a smooth function and converge fast
		constexpr uint32_t k_SpecularSampleCount = 128;
		constexpr uint32_t k_IrradianceSampleCount = 256;
		constexpr uint32_t k_BrdfSampleCount = 512;

		bool SameColor(const Color& a, const Color& b)
		{
			return a.R == b.R && a.G == b.G && a.B == b.B && a.A == b.A;
		}
	}

	EnvironmentPass::~EnvironmentPass()
	{
		Shutdown();
	}

	bool EnvironmentPass::Initialize(D3D12Renderer& renderer)
	{
		D3D12PipelineCache& l_Cache = renderer.GetPipelineCache();
		m_RootSignature = l_Cache.GetRootSignature();

		// Fullscreen draws into single-sample RGBA16F faces, no depth, no culling; the BRDF table differs only in its format
		GraphicsPipelineDescription l_Description;
		l_Description.VertexShader = "Environment.VSMain";
		l_Description.PixelShader = "Environment.PSSpecular";
		l_Description.RenderTargetFormats[0] = D3D12EnvironmentMaps::k_Format;
		l_Description.RenderTargetCount = 1;
		l_Description.CullMode = D3D12_CULL_MODE_NONE;

		m_SpecularPipeline = l_Cache.GetGraphicsPipeline(l_Description);

		l_Description.PixelShader = "Environment.PSIrradiance";
		m_IrradiancePipeline = l_Cache.GetGraphicsPipeline(l_Description);

		l_Description.PixelShader = "Environment.PSBrdf";
		l_Description.RenderTargetFormats[0] = D3D12EnvironmentMaps::k_BrdfFormat;
		m_BrdfPipeline = l_Cache.GetGraphicsPipeline(l_Description);

		if (m_SpecularPipeline == nullptr || m_IrradiancePipeline == nullptr || m_BrdfPipeline == nullptr)
		{
			Shutdown();

			return false;
		}

		m_DrawCalls = 0;
		m_BakeCount = 0;
		m_Baked = false;
		m_BrdfBaked = false;
		m_Initialized = true;

		PT_CORE_INFO("Environment pass ready");

		return true;
	}

	void EnvironmentPass::Shutdown()
	{
		m_SpecularPipeline = nullptr;
		m_IrradiancePipeline = nullptr;
		m_BrdfPipeline = nullptr;
		m_RootSignature = nullptr;
		m_DrawCalls = 0;
		m_Baked = false;
		m_BrdfBaked = false;

		if (m_Initialized)
		{
			PT_CORE_INFO("Environment pass shut down after {} bakes", m_BakeCount);
		}

		m_BakeCount = 0;
		m_Initialized = false;
	}

	bool EnvironmentPass::NeedsBake(const SunLight& sun, const Color& skyTint) const
	{
		if (!m_Initialized)
		{
			return false;
		}

		// Exact comparison on purpose: a static sun bakes once, a moving sun rebakes every frame either way
		return !m_Baked || sun.TowardsSun != m_BakedTowardsSun || sun.Radiance != m_BakedRadiance || !SameColor(skyTint, m_BakedTint);
	}

	void EnvironmentPass::Render(ID3D12GraphicsCommandList* commandList, D3D12EnvironmentMaps& maps, D3D12UploadRing& uploadRing, D3D12_GPU_VIRTUAL_ADDRESS frameConstants, const SunLight& sun, const Color& skyTint)
	{
		m_DrawCalls = 0;

		if (!m_Initialized || frameConstants == 0)
		{
			return;
		}

		maps.TransitionTo(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

		commandList->SetGraphicsRootSignature(m_RootSignature);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->SetGraphicsRootConstantBufferView(D3D12PipelineCache::k_FrameConstantsParameter, frameConstants);

		bool l_Complete = true;

		// The BRDF table depends on nothing in the scene, so it is written once and kept
		if (!m_BrdfBaked)
		{
			commandList->SetPipelineState(m_BrdfPipeline);
			l_Complete = DrawFace(commandList, uploadRing, maps.GetBrdfRtv(), D3D12EnvironmentMaps::k_BrdfSize, 0, k_BrdfSampleCount, 0.0f) && l_Complete;
		}

		// One mip per roughness step, the same mapping the forward shader uses to pick its level
		commandList->SetPipelineState(m_SpecularPipeline);
		for (uint32_t l_Mip = 0; l_Mip < D3D12EnvironmentMaps::k_SpecularMipCount; ++l_Mip)
		{
			const float l_Roughness = static_cast<float>(l_Mip) / static_cast<float>(D3D12EnvironmentMaps::k_SpecularMipCount - 1);
			const uint32_t l_SampleCount = l_Mip == 0 ? 1 : k_SpecularSampleCount;
			for (uint32_t l_Face = 0; l_Face < D3D12EnvironmentMaps::k_FaceCount; ++l_Face)
			{
				l_Complete = DrawFace(commandList, uploadRing, maps.GetSpecularRtv(l_Face, l_Mip), D3D12EnvironmentMaps::GetSpecularMipSize(l_Mip), l_Face, l_SampleCount, l_Roughness) && l_Complete;
			}
		}

		commandList->SetPipelineState(m_IrradiancePipeline);
		for (uint32_t l_Face = 0; l_Face < D3D12EnvironmentMaps::k_FaceCount; ++l_Face)
		{
			l_Complete = DrawFace(commandList, uploadRing, maps.GetIrradianceRtv(l_Face), D3D12EnvironmentMaps::k_IrradianceSize, l_Face, k_IrradianceSampleCount, 0.0f) && l_Complete;
		}

		maps.TransitionTo(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		// A bake the ring could not feed runs again next frame rather than leaving a stale face behind
		if (l_Complete)
		{
			m_BakedTowardsSun = sun.TowardsSun;
			m_BakedRadiance = sun.Radiance;
			m_BakedTint = skyTint;
			m_Baked = true;
			m_BrdfBaked = true;
			++m_BakeCount;
		}
	}

	bool EnvironmentPass::DrawFace(ID3D12GraphicsCommandList* commandList, D3D12UploadRing& uploadRing, D3D12_CPU_DESCRIPTOR_HANDLE target, uint32_t size, uint32_t face, uint32_t sampleCount, float roughness)
	{
		ShaderInterop::EnvironmentPassConstants l_Constants = {};
		l_Constants.Target.X = face;
		l_Constants.Target.Y = sampleCount;
		l_Constants.Target.Z = size;
		l_Constants.Filter = { roughness, 1.0f / static_cast<float>(size), 0.0f, 0.0f };

		const UploadAllocation l_Allocation = uploadRing.Upload(l_Constants);
		if (!l_Allocation.IsValid())
		{
			return false;
		}

		commandList->OMSetRenderTargets(1, &target, FALSE, nullptr);

		D3D12_VIEWPORT l_Viewport = {};
		l_Viewport.Width = static_cast<float>(size);
		l_Viewport.Height = static_cast<float>(size);
		l_Viewport.MinDepth = 0.0f;
		l_Viewport.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &l_Viewport);

		const D3D12_RECT l_Scissor = { 0, 0, static_cast<LONG>(size), static_cast<LONG>(size) };
		commandList->RSSetScissorRects(1, &l_Scissor);

		commandList->SetGraphicsRootConstantBufferView(D3D12PipelineCache::k_PassConstantsParameter, l_Allocation.Gpu);
		commandList->DrawInstanced(3, 1, 0, 0);
		++m_DrawCalls;

		return true;
	}
}
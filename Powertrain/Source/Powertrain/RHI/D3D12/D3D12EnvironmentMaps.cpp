#include "Powertrain/RHI/D3D12/D3D12EnvironmentMaps.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

#include <format>

namespace Powertrain
{
	D3D12EnvironmentMaps::~D3D12EnvironmentMaps()
	{
		Shutdown();
	}

	bool D3D12EnvironmentMaps::Initialize(D3D12Device& device, D3D12DescriptorHeap& rtvHeap, D3D12DescriptorHeap& resourceHeap)
	{
		PT_CORE_ASSERT(rtvHeap.GetType() == D3D12_DESCRIPTOR_HEAP_TYPE_RTV, "Environment maps need the RTV heap");
		PT_CORE_ASSERT(resourceHeap.IsShaderVisible(), "Environment maps need the shader-visible resource heap");
		static_assert(GetSpecularMipSize(k_SpecularMipCount - 1) >= 1, "The specular cube has more mips than its size allows");

		m_Device = &device;
		m_RtvHeap = &rtvHeap;
		m_ResourceHeap = &resourceHeap;

		ID3D12Device* l_Device = device.GetHandle();

		if (!CreateCube(m_Specular.ReleaseAndGetAddressOf(), k_SpecularSize, k_SpecularMipCount, "Sky Specular Cube") || !CreateCube(m_Irradiance.ReleaseAndGetAddressOf(), k_IrradianceSize, 1, "Sky Irradiance Cube"))
		{
			Shutdown();

			return false;
		}

		D3D12_HEAP_PROPERTIES l_HeapProperties = {};
		l_HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC l_BrdfDescription = {};
		l_BrdfDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		l_BrdfDescription.Width = k_BrdfSize;
		l_BrdfDescription.Height = k_BrdfSize;
		l_BrdfDescription.DepthOrArraySize = 1;
		l_BrdfDescription.MipLevels = 1;
		l_BrdfDescription.Format = k_BrdfFormat;
		l_BrdfDescription.SampleDesc = { 1, 0 };
		l_BrdfDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		l_BrdfDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		if (!D3D12::CheckResult(l_Device->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_BrdfDescription, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&m_Brdf)), "CreateCommittedResource (BRDF table)"))
		{
			Shutdown();

			return false;
		}

		D3D12::SetDebugName(m_Brdf.Get(), "Sky BRDF Table");
		m_State = D3D12_RESOURCE_STATE_RENDER_TARGET;

		// One render target view per face and mip, so the bake is a run of independent fullscreen draws
		for (uint32_t l_Mip = 0; l_Mip < k_SpecularMipCount; ++l_Mip)
		{
			for (uint32_t l_Face = 0; l_Face < k_FaceCount; ++l_Face)
			{
				DescriptorHandle& l_Rtv = m_SpecularRtvs[l_Mip * k_FaceCount + l_Face];
				l_Rtv = rtvHeap.Allocate();
				if (!l_Rtv.IsValid())
				{
					Shutdown();

					return false;
				}

				D3D12_RENDER_TARGET_VIEW_DESC l_View = {};
				l_View.Format = k_Format;
				l_View.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				l_View.Texture2DArray.MipSlice = l_Mip;
				l_View.Texture2DArray.FirstArraySlice = l_Face;
				l_View.Texture2DArray.ArraySize = 1;
				l_View.Texture2DArray.PlaneSlice = 0;
				l_Device->CreateRenderTargetView(m_Specular.Get(), &l_View, l_Rtv.CPU);
			}
		}

		for (uint32_t l_Face = 0; l_Face < k_FaceCount; ++l_Face)
		{
			m_IrradianceRtvs[l_Face] = rtvHeap.Allocate();
			if (!m_IrradianceRtvs[l_Face].IsValid())
			{
				Shutdown();

				return false;
			}

			D3D12_RENDER_TARGET_VIEW_DESC l_View = {};
			l_View.Format = k_Format;
			l_View.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			l_View.Texture2DArray.MipSlice = 0;
			l_View.Texture2DArray.FirstArraySlice = l_Face;
			l_View.Texture2DArray.ArraySize = 1;
			l_View.Texture2DArray.PlaneSlice = 0;
			l_Device->CreateRenderTargetView(m_Irradiance.Get(), &l_View, m_IrradianceRtvs[l_Face].CPU);
		}

		m_BrdfRtv = rtvHeap.Allocate();
		if (!m_BrdfRtv.IsValid())
		{
			Shutdown();

			return false;
		}

		D3D12_RENDER_TARGET_VIEW_DESC l_BrdfRtv = {};
		l_BrdfRtv.Format = k_BrdfFormat;
		l_BrdfRtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		l_BrdfRtv.Texture2D.MipSlice = 0;
		l_BrdfRtv.Texture2D.PlaneSlice = 0;
		l_Device->CreateRenderTargetView(m_Brdf.Get(), &l_BrdfRtv, m_BrdfRtv.CPU);

		// The shader views: two cubes and one table, all in persistent slots the frame constants carry
		m_SpecularView = resourceHeap.Allocate();
		m_IrradianceView = resourceHeap.Allocate();
		m_BrdfView = resourceHeap.Allocate();
		if (!m_SpecularView.IsValid() || !m_IrradianceView.IsValid() || !m_BrdfView.IsValid())
		{
			Shutdown();

			return false;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC l_CubeView = {};
		l_CubeView.Format = k_Format;
		l_CubeView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		l_CubeView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		l_CubeView.TextureCube.MostDetailedMip = 0;
		l_CubeView.TextureCube.MipLevels = k_SpecularMipCount;
		l_CubeView.TextureCube.ResourceMinLODClamp = 0.0f;
		l_Device->CreateShaderResourceView(m_Specular.Get(), &l_CubeView, m_SpecularView.CPU);

		l_CubeView.TextureCube.MipLevels = 1;
		l_Device->CreateShaderResourceView(m_Irradiance.Get(), &l_CubeView, m_IrradianceView.CPU);

		D3D12_SHADER_RESOURCE_VIEW_DESC l_BrdfView = {};
		l_BrdfView.Format = k_BrdfFormat;
		l_BrdfView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		l_BrdfView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		l_BrdfView.Texture2D.MostDetailedMip = 0;
		l_BrdfView.Texture2D.MipLevels = 1;
		l_BrdfView.Texture2D.PlaneSlice = 0;
		l_BrdfView.Texture2D.ResourceMinLODClamp = 0.0f;
		l_Device->CreateShaderResourceView(m_Brdf.Get(), &l_BrdfView, m_BrdfView.CPU);

		m_Initialized = true;

		PT_CORE_INFO("Environment maps ready: specular {} px with {} mips, irradiance {} px, BRDF table {} px, descriptors {} {} {}", k_SpecularSize, k_SpecularMipCount, k_IrradianceSize, k_BrdfSize, m_SpecularView.Index, m_IrradianceView.Index, m_BrdfView.Index);

		return true;
	}

	void D3D12EnvironmentMaps::Shutdown()
	{
		if (m_RtvHeap != nullptr)
		{
			for (DescriptorHandle& l_Rtv : m_SpecularRtvs)
			{
				m_RtvHeap->Free(l_Rtv);
			}

			for (DescriptorHandle& l_Rtv : m_IrradianceRtvs)
			{
				m_RtvHeap->Free(l_Rtv);
			}

			m_RtvHeap->Free(m_BrdfRtv);
		}

		if (m_ResourceHeap != nullptr)
		{
			m_ResourceHeap->Free(m_SpecularView);
			m_ResourceHeap->Free(m_IrradianceView);
			m_ResourceHeap->Free(m_BrdfView);
		}

		m_Specular.Reset();
		m_Irradiance.Reset();
		m_Brdf.Reset();
		m_Device = nullptr;
		m_RtvHeap = nullptr;
		m_ResourceHeap = nullptr;

		if (m_Initialized)
		{
			PT_CORE_INFO("Environment maps shut down");
		}

		m_Initialized = false;
	}

	void D3D12EnvironmentMaps::TransitionTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state)
	{
		if (!m_Initialized || state == m_State)
		{
			return;
		}

		D3D12_RESOURCE_BARRIER l_Barriers[3] = {};
		ID3D12Resource* l_Resources[3] = { m_Specular.Get(), m_Irradiance.Get(), m_Brdf.Get() };
		for (uint32_t l_Index = 0; l_Index < 3; ++l_Index)
		{
			l_Barriers[l_Index].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			l_Barriers[l_Index].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			l_Barriers[l_Index].Transition.pResource = l_Resources[l_Index];
			l_Barriers[l_Index].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			l_Barriers[l_Index].Transition.StateBefore = m_State;
			l_Barriers[l_Index].Transition.StateAfter = state;
		}
		commandList->ResourceBarrier(3, l_Barriers);

		m_State = state;
	}

	bool D3D12EnvironmentMaps::CreateCube(ID3D12Resource** resource, uint32_t size, uint32_t mipCount, const char* name)
	{
		D3D12_HEAP_PROPERTIES l_HeapProperties = {};
		l_HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		// A cube is a six-slice array; the bake never clears it, every texel is written by a fullscreen draw
		D3D12_RESOURCE_DESC l_Description = {};
		l_Description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		l_Description.Width = size;
		l_Description.Height = size;
		l_Description.DepthOrArraySize = static_cast<UINT16>(k_FaceCount);
		l_Description.MipLevels = static_cast<UINT16>(mipCount);
		l_Description.Format = k_Format;
		l_Description.SampleDesc = { 1, 0 };
		l_Description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		l_Description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		if (!D3D12::CheckResult(m_Device->GetHandle()->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_Description, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(resource)), std::format("CreateCommittedResource ({})", name)))
		{
			return false;
		}

		D3D12::SetDebugName(*resource, name);

		return true;
	}
}
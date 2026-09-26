#include "Powertrain/RHI/D3D12/D3D12ShadowMap.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

namespace Powertrain
{
	D3D12ShadowMap::~D3D12ShadowMap()
	{
		Shutdown();
	}

	bool D3D12ShadowMap::Initialize(D3D12Device& device, D3D12DescriptorHeap& dsvHeap, D3D12DescriptorHeap& resourceHeap)
	{
		PT_CORE_ASSERT(dsvHeap.GetType() == D3D12_DESCRIPTOR_HEAP_TYPE_DSV, "Shadow map needs the DSV heap");
		PT_CORE_ASSERT(resourceHeap.IsShaderVisible(), "Shadow map needs the shader-visible resource heap");

		m_Device = &device;
		m_DsvHeap = &dsvHeap;
		m_ResourceHeap = &resourceHeap;

		D3D12_HEAP_PROPERTIES l_HeapProperties = {};
		l_HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		// Typeless so the same memory serves the D32 depth views and the R32 float shader view
		D3D12_RESOURCE_DESC l_Description = {};
		l_Description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		l_Description.Width = k_Size;
		l_Description.Height = k_Size;
		l_Description.DepthOrArraySize = static_cast<UINT16>(k_CascadeCount);
		l_Description.MipLevels = 1;
		l_Description.Format = k_ResourceFormat;
		l_Description.SampleDesc = { 1, 0 };
		l_Description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		l_Description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE l_ClearValue = {};
		l_ClearValue.Format = k_DepthFormat;
		l_ClearValue.DepthStencil.Depth = k_ClearDepth;
		l_ClearValue.DepthStencil.Stencil = 0;

		if (!D3D12::CheckResult(device.GetHandle()->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_Description, D3D12_RESOURCE_STATE_DEPTH_WRITE, &l_ClearValue, IID_PPV_ARGS(&m_Resource)), "CreateCommittedResource (shadow map)"))
		{
			Shutdown();

			return false;
		}

		D3D12::SetDebugName(m_Resource.Get(), "Sun Shadow Map");
		m_State = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		for (uint32_t l_Cascade = 0; l_Cascade < k_CascadeCount; ++l_Cascade)
		{
			m_Dsvs[l_Cascade] = dsvHeap.Allocate();
			if (!m_Dsvs[l_Cascade].IsValid())
			{
				Shutdown();

				return false;
			}

			D3D12_DEPTH_STENCIL_VIEW_DESC l_View = {};
			l_View.Format = k_DepthFormat;
			l_View.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			l_View.Flags = D3D12_DSV_FLAG_NONE;
			l_View.Texture2DArray.MipSlice = 0;
			l_View.Texture2DArray.FirstArraySlice = l_Cascade;
			l_View.Texture2DArray.ArraySize = 1;

			device.GetHandle()->CreateDepthStencilView(m_Resource.Get(), &l_View, m_Dsvs[l_Cascade].CPU);
		}

		m_ShaderResource = resourceHeap.Allocate();
		if (!m_ShaderResource.IsValid())
		{
			Shutdown();

			return false;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC l_ShaderView = {};
		l_ShaderView.Format = k_ShaderFormat;
		l_ShaderView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		l_ShaderView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		l_ShaderView.Texture2DArray.MostDetailedMip = 0;
		l_ShaderView.Texture2DArray.MipLevels = 1;
		l_ShaderView.Texture2DArray.FirstArraySlice = 0;
		l_ShaderView.Texture2DArray.ArraySize = k_CascadeCount;
		l_ShaderView.Texture2DArray.PlaneSlice = 0;
		l_ShaderView.Texture2DArray.ResourceMinLODClamp = 0.0f;

		device.GetHandle()->CreateShaderResourceView(m_Resource.Get(), &l_ShaderView, m_ShaderResource.CPU);

		m_Initialized = true;

		PT_CORE_INFO("Shadow map ready: {} cascades of {}x{} D32, descriptor {}", k_CascadeCount, k_Size, k_Size, m_ShaderResource.Index);

		return true;
	}

	void D3D12ShadowMap::Shutdown()
	{
		if (m_DsvHeap != nullptr)
		{
			for (DescriptorHandle& l_Dsv : m_Dsvs)
			{
				m_DsvHeap->Free(l_Dsv);
			}
		}

		if (m_ResourceHeap != nullptr)
		{
			m_ResourceHeap->Free(m_ShaderResource);
		}

		m_Resource.Reset();
		m_Device = nullptr;
		m_DsvHeap = nullptr;
		m_ResourceHeap = nullptr;

		if (m_Initialized)
		{
			PT_CORE_INFO("Shadow map shut down");
		}

		m_Initialized = false;
	}

	void D3D12ShadowMap::TransitionTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state)
	{
		if (!m_Initialized || state == m_State)
		{
			return;
		}

		D3D12_RESOURCE_BARRIER l_Barrier = {};
		l_Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		l_Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		l_Barrier.Transition.pResource = m_Resource.Get();
		l_Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		l_Barrier.Transition.StateBefore = m_State;
		l_Barrier.Transition.StateAfter = state;
		commandList->ResourceBarrier(1, &l_Barrier);

		m_State = state;
	}

	void D3D12ShadowMap::Clear(ID3D12GraphicsCommandList* commandList, uint32_t cascade) const
	{
		PT_CORE_ASSERT(cascade < k_CascadeCount, "Shadow cascade {} out of range", cascade);

		commandList->ClearDepthStencilView(m_Dsvs[cascade].CPU, D3D12_CLEAR_FLAG_DEPTH, k_ClearDepth, 0, 0, nullptr);
	}
}
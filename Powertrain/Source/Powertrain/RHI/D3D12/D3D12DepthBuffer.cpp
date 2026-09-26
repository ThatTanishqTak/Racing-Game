#include "Powertrain/RHI/D3D12/D3D12DepthBuffer.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

namespace Powertrain
{
	D3D12DepthBuffer::~D3D12DepthBuffer()
	{
		Shutdown();
	}

	bool D3D12DepthBuffer::Initialize(D3D12Device& device, D3D12DescriptorHeap& dsvHeap, uint32_t width, uint32_t height, uint32_t sampleCount)
	{
		PT_CORE_ASSERT(dsvHeap.GetType() == D3D12_DESCRIPTOR_HEAP_TYPE_DSV, "Depth buffer needs the DSV heap");
		PT_CORE_ASSERT(sampleCount > 0, "Depth buffer sample count {} is invalid", sampleCount);

		m_Device = &device;
		m_DsvHeap = &dsvHeap;
		m_SampleCount = sampleCount;

		if (!Create(width, height))
		{
			Shutdown();

			return false;
		}

		PT_CORE_INFO("Depth buffer ready: {}x{} D32, {}x MSAA", width, height, m_SampleCount);

		return true;
	}

	void D3D12DepthBuffer::Shutdown()
	{
		Release();

		m_Device = nullptr;
		m_DsvHeap = nullptr;
		m_SampleCount = 1;
	}


	bool D3D12DepthBuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == m_Width && height == m_Height)
		{
			return true;
		}

		Release();

		return Create(width, height);
	}

	void D3D12DepthBuffer::Clear(ID3D12GraphicsCommandList* commandList) const
	{
		commandList->ClearDepthStencilView(m_Dsv.CPU, D3D12_CLEAR_FLAG_DEPTH, k_ClearDepth, 0, 0, nullptr);
	}

	bool D3D12DepthBuffer::Create(uint32_t width, uint32_t height)
	{
		PT_CORE_ASSERT(width > 0 && height > 0, "Depth buffer size {}x{} is empty", width, height);

		D3D12_HEAP_PROPERTIES l_HeapProperties = {};
		l_HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC l_Description = {};
		l_Description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		l_Description.Width = width;
		l_Description.Height = height;
		l_Description.DepthOrArraySize = 1;
		l_Description.MipLevels = 1;
		l_Description.Format = k_Format;
		l_Description.SampleDesc = { m_SampleCount, 0 };
		l_Description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		l_Description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		// The optimized clear value must match every ClearDepthStencilView, and reversed-Z clears to 0
		D3D12_CLEAR_VALUE l_ClearValue = {};
		l_ClearValue.Format = k_Format;
		l_ClearValue.DepthStencil.Depth = k_ClearDepth;
		l_ClearValue.DepthStencil.Stencil = 0;

		if (!D3D12::CheckResult(m_Device->GetHandle()->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_Description, D3D12_RESOURCE_STATE_DEPTH_WRITE, &l_ClearValue, IID_PPV_ARGS(&m_Resource)), "CreateCommittedResource (depth buffer)"))
		{
			return false;
		}

		D3D12::SetDebugName(m_Resource.Get(), "Depth Buffer");

		m_Dsv = m_DsvHeap->Allocate();
		if (!m_Dsv.IsValid())
		{
			return false;
		}

		// A multisampled view has no mip to pick; the single-sample view names mip 0
		D3D12_DEPTH_STENCIL_VIEW_DESC l_View = {};
		l_View.Format = k_Format;
		l_View.ViewDimension = m_SampleCount > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
		l_View.Flags = D3D12_DSV_FLAG_NONE;
		l_View.Texture2D.MipSlice = 0;

		m_Device->GetHandle()->CreateDepthStencilView(m_Resource.Get(), &l_View, m_Dsv.CPU);

		m_Width = width;
		m_Height = height;

		return true;
	}

	void D3D12DepthBuffer::Release()
	{
		if (m_DsvHeap != nullptr)
		{
			m_DsvHeap->Free(m_Dsv);
		}

		m_Resource.Reset();
		m_Width = 0;
		m_Height = 0;
	}
}
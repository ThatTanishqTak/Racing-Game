#include "Powertrain/RHI/D3D12/D3D12SceneTarget.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12DepthBuffer.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

namespace Powertrain
{
	namespace
	{
		D3D12_RESOURCE_BARRIER MakeTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
		{
			D3D12_RESOURCE_BARRIER l_Barrier = {};
			l_Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			l_Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			l_Barrier.Transition.pResource = resource;
			l_Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			l_Barrier.Transition.StateBefore = before;
			l_Barrier.Transition.StateAfter = after;

			return l_Barrier;
		}

		bool SameColor(const Color& a, const Color& b)
		{
			return a.R == b.R && a.G == b.G && a.B == b.B && a.A == b.A;
		}
	}

	D3D12SceneTarget::~D3D12SceneTarget()
	{
		Shutdown();
	}

	bool D3D12SceneTarget::Initialize(D3D12Device& device, D3D12DescriptorHeap& rtvHeap, D3D12DescriptorHeap& resourceHeap, uint32_t width, uint32_t height, const Color& clearColor)
	{
		PT_CORE_ASSERT(rtvHeap.GetType() == D3D12_DESCRIPTOR_HEAP_TYPE_RTV, "Scene target needs the RTV heap");
		PT_CORE_ASSERT(resourceHeap.IsShaderVisible(), "Scene target needs the shader-visible resource heap");

		m_Device = &device;
		m_RtvHeap = &rtvHeap;
		m_ResourceHeap = &resourceHeap;
		m_SampleCount = QuerySampleCount();

		if (!Create(width, height, clearColor))
		{
			Shutdown();

			return false;
		}

		m_Initialized = true;

		PT_CORE_INFO("Scene target ready: {}x{} RGBA16F, {}x MSAA", width, height, m_SampleCount);

		return true;
	}

	void D3D12SceneTarget::Shutdown()
	{
		Release();

		m_Device = nullptr;
		m_RtvHeap = nullptr;
		m_ResourceHeap = nullptr;
		m_SampleCount = 1;

		if (m_Initialized)
		{
			PT_CORE_INFO("Scene target shut down");
		}

		m_Initialized = false;
	}

	bool D3D12SceneTarget::Resize(uint32_t width, uint32_t height, const Color& clearColor)
	{
		if (width == m_Width && height == m_Height && SameColor(clearColor, m_ClearColor))
		{
			return true;
		}

		Release();

		return Create(width, height, clearColor);
	}

	void D3D12SceneTarget::Clear(ID3D12GraphicsCommandList* commandList) const
	{
		const float l_Clear[4] = { m_ClearColor.R, m_ClearColor.G, m_ClearColor.B, m_ClearColor.A };
		commandList->ClearRenderTargetView(m_Rtv.CPU, l_Clear, 0, nullptr);
	}

	void D3D12SceneTarget::Resolve(ID3D12GraphicsCommandList* commandList)
	{
		if (!m_Initialized)
		{
			return;
		}

		// A single-sample target has nothing to resolve, so the copy path keeps the post pass reading one place
		const D3D12_RESOURCE_STATES l_SourceState = m_SampleCount > 1 ? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;
		const D3D12_RESOURCE_STATES l_DestinationState = m_SampleCount > 1 ? D3D12_RESOURCE_STATE_RESOLVE_DEST : D3D12_RESOURCE_STATE_COPY_DEST;

		const D3D12_RESOURCE_BARRIER l_Before[2] =
		{
			MakeTransition(m_Color.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, l_SourceState),
			MakeTransition(m_Resolved.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, l_DestinationState)
		};
		commandList->ResourceBarrier(2, l_Before);

		if (m_SampleCount > 1)
		{
			commandList->ResolveSubresource(m_Resolved.Get(), 0, m_Color.Get(), 0, k_Format);
		}
		else
		{
			commandList->CopyResource(m_Resolved.Get(), m_Color.Get());
		}

		const D3D12_RESOURCE_BARRIER l_After[2] =
		{
			MakeTransition(m_Color.Get(), l_SourceState, D3D12_RESOURCE_STATE_RENDER_TARGET),
			MakeTransition(m_Resolved.Get(), l_DestinationState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		};
		commandList->ResourceBarrier(2, l_After);
	}

	bool D3D12SceneTarget::Create(uint32_t width, uint32_t height, const Color& clearColor)
	{
		PT_CORE_ASSERT(width > 0 && height > 0, "Scene target size {}x{} is empty", width, height);

		ID3D12Device* l_Device = m_Device->GetHandle();

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
		l_Description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		// The optimized clear value must match every ClearRenderTargetView, so a new clear colour recreates the target
		D3D12_CLEAR_VALUE l_ClearValue = {};
		l_ClearValue.Format = k_Format;
		l_ClearValue.Color[0] = clearColor.R;
		l_ClearValue.Color[1] = clearColor.G;
		l_ClearValue.Color[2] = clearColor.B;
		l_ClearValue.Color[3] = clearColor.A;

		if (!D3D12::CheckResult(l_Device->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_Description, D3D12_RESOURCE_STATE_RENDER_TARGET, &l_ClearValue, IID_PPV_ARGS(&m_Color)), "CreateCommittedResource (scene colour)"))
		{
			return false;
		}

		D3D12::SetDebugName(m_Color.Get(), "Scene Colour");

		m_Rtv = m_RtvHeap->Allocate();
		if (!m_Rtv.IsValid())
		{
			return false;
		}

		D3D12_RENDER_TARGET_VIEW_DESC l_RtvDescription = {};
		l_RtvDescription.Format = k_Format;
		l_RtvDescription.ViewDimension = m_SampleCount > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
		l_Device->CreateRenderTargetView(m_Color.Get(), &l_RtvDescription, m_Rtv.CPU);

		// The resolved copy is only ever a resolve destination and a shader input; it rests as the latter for the post pass
		l_Description.SampleDesc = { 1, 0 };
		l_Description.Flags = D3D12_RESOURCE_FLAG_NONE;

		if (!D3D12::CheckResult(l_Device->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_Description, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&m_Resolved)), "CreateCommittedResource (scene colour resolved)"))
		{
			return false;
		}

		D3D12::SetDebugName(m_Resolved.Get(), "Scene Colour Resolved");

		m_ResolvedView = m_ResourceHeap->Allocate();
		if (!m_ResolvedView.IsValid())
		{
			return false;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC l_View = {};
		l_View.Format = k_Format;
		l_View.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		l_View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		l_View.Texture2D.MostDetailedMip = 0;
		l_View.Texture2D.MipLevels = 1;
		l_View.Texture2D.PlaneSlice = 0;
		l_View.Texture2D.ResourceMinLODClamp = 0.0f;
		l_Device->CreateShaderResourceView(m_Resolved.Get(), &l_View, m_ResolvedView.CPU);

		m_ClearColor = clearColor;
		m_Width = width;
		m_Height = height;

		return true;
	}

	void D3D12SceneTarget::Release()
	{
		if (m_RtvHeap != nullptr)
		{
			m_RtvHeap->Free(m_Rtv);
		}

		if (m_ResourceHeap != nullptr)
		{
			m_ResourceHeap->Free(m_ResolvedView);
		}

		m_Color.Reset();
		m_Resolved.Reset();
		m_Width = 0;
		m_Height = 0;
	}

	uint32_t D3D12SceneTarget::QuerySampleCount() const
	{
		// Both the colour and the depth format must take the sample count, or the pipeline cannot pair them
		const DXGI_FORMAT l_Formats[2] = { k_Format, D3D12DepthBuffer::k_Format };
		for (DXGI_FORMAT l_Format : l_Formats)
		{
			D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS l_Levels = {};
			l_Levels.Format = l_Format;
			l_Levels.SampleCount = k_PreferredSampleCount;
			l_Levels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

			if (FAILED(m_Device->GetHandle()->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &l_Levels, sizeof(l_Levels))) || l_Levels.NumQualityLevels == 0)
			{
				PT_CORE_WARN("Adapter does not support {}x MSAA for format {}; the scene renders single-sampled", k_PreferredSampleCount, static_cast<int>(l_Format));

				return 1;
			}
		}

		return k_PreferredSampleCount;
	}
}
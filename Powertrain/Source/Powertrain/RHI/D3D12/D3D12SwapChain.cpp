#include "Powertrain/RHI/D3D12/D3D12SwapChain.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12CommandQueue.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

#include <algorithm>
#include <format>

namespace Powertrain
{
	namespace
	{
		bool IsDeviceRemoved(HRESULT result)
		{
			return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET || result == DXGI_ERROR_DEVICE_HUNG || result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
		}
	}

	D3D12SwapChain::~D3D12SwapChain()
	{
		Shutdown();
	}

	bool D3D12SwapChain::Initialize(D3D12Device& device, D3D12CommandQueue& queue, D3D12DescriptorHeap& rtvHeap, HWND windowHandle, uint32_t width, uint32_t height, bool vsync)
	{
		PT_CORE_ASSERT(queue.GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT, "Swap chain needs the direct queue");
		PT_CORE_ASSERT(rtvHeap.GetType() == D3D12_DESCRIPTOR_HEAP_TYPE_RTV, "Swap chain needs the RTV heap");

		m_Device = &device;
		m_Queue = &queue;
		m_RtvHeap = &rtvHeap;
		m_VSync = vsync;
		m_Tearing = device.GetCapabilities().Tearing;
		m_Width = std::max(width, 1u);
		m_Height = std::max(height, 1u);

		DXGI_SWAP_CHAIN_DESC1 l_Description = {};
		l_Description.Width = m_Width;
		l_Description.Height = m_Height;
		l_Description.Format = k_Format;
		l_Description.Stereo = FALSE;
		l_Description.SampleDesc = { 1, 0 };
		l_Description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		l_Description.BufferCount = k_BufferCount;
		l_Description.Scaling = DXGI_SCALING_STRETCH;
		l_Description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		l_Description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		l_Description.Flags = GetFlags();

		ComPtr<IDXGISwapChain1> l_SwapChain1;
		if (!D3D12::CheckResult(device.GetFactory()->CreateSwapChainForHwnd(queue.GetHandle(), windowHandle, &l_Description, nullptr, nullptr, &l_SwapChain1), "CreateSwapChainForHwnd"))
		{
			return false;
		}

		// Borderless fullscreen is handled by WindowsWindow; DXGI must not react to Alt+Enter itself
		device.GetFactory()->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER);

		if (!D3D12::CheckResult(l_SwapChain1.As(&m_SwapChain), "IDXGISwapChain1 to IDXGISwapChain4"))
		{
			return false;
		}

		if (!D3D12::CheckResult(m_SwapChain->SetMaximumFrameLatency(D3D12::k_FramesInFlight), "SetMaximumFrameLatency"))
		{
			return false;
		}

		m_WaitableObject = m_SwapChain->GetFrameLatencyWaitableObject();
		if (m_WaitableObject == nullptr)
		{
			PT_CORE_ERROR("GetFrameLatencyWaitableObject returned null");

			return false;
		}

		if (!CreateBuffers())
		{
			return false;
		}

		PT_CORE_INFO("Swap chain created: {}x{}, {} buffers, VSync {}, tearing {}", m_Width, m_Height, k_BufferCount, m_VSync ? "on" : "off", m_Tearing ? "allowed" : "unavailable");

		return true;
	}

	void D3D12SwapChain::Shutdown()
	{
		if (m_Queue != nullptr && m_Queue->GetHandle() != nullptr)
		{
			m_Queue->Flush();
		}

		ReleaseBuffers();

		if (m_WaitableObject != nullptr)
		{
			CloseHandle(m_WaitableObject);
			m_WaitableObject = nullptr;
		}

		m_SwapChain.Reset();

		m_Device = nullptr;
		m_Queue = nullptr;
		m_RtvHeap = nullptr;
		m_Width = 0;
		m_Height = 0;
		m_BufferIndex = 0;
	}

	void D3D12SwapChain::WaitForNextFrame()
	{
		if (m_WaitableObject != nullptr)
		{
			// A bounded wait so a stalled compositor cannot hang the loop forever
			WaitForSingleObjectEx(m_WaitableObject, 1000, TRUE);
		}
	}

	bool D3D12SwapChain::Present()
	{
		const UINT l_SyncInterval = m_VSync ? 1 : 0;
		const UINT l_Flags = (!m_VSync && m_Tearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;

		const HRESULT l_Result = m_SwapChain->Present(l_SyncInterval, l_Flags);
		if (IsDeviceRemoved(l_Result))
		{
			m_Device->ReportDeviceRemoved();

			return false;
		}

		D3D12::CheckResult(l_Result, "IDXGISwapChain::Present");
		m_BufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

		return true;
	}

	bool D3D12SwapChain::Resize(uint32_t width, uint32_t height)
	{
		width = std::max(width, 1u);
		height = std::max(height, 1u);

		if (width == m_Width && height == m_Height)
		{
			return true;
		}

		m_Queue->Flush();
		ReleaseBuffers();

		const HRESULT l_Result = m_SwapChain->ResizeBuffers(k_BufferCount, width, height, k_Format, GetFlags());
		if (IsDeviceRemoved(l_Result))
		{
			m_Device->ReportDeviceRemoved();

			return false;
		}

		if (!D3D12::CheckResult(l_Result, "IDXGISwapChain::ResizeBuffers"))
		{
			return false;
		}

		m_Width = width;
		m_Height = height;

		if (!CreateBuffers())
		{
			return false;
		}

		PT_CORE_TRACE("Swap chain resized to {}x{}", m_Width, m_Height);

		return true;
	}

	bool D3D12SwapChain::CreateBuffers()
	{
		for (uint32_t l_Index = 0; l_Index < k_BufferCount; ++l_Index)
		{
			if (!D3D12::CheckResult(m_SwapChain->GetBuffer(l_Index, IID_PPV_ARGS(&m_Buffers[l_Index])), std::format("IDXGISwapChain::GetBuffer {}", l_Index)))
			{
				return false;
			}

			D3D12::SetDebugName(m_Buffers[l_Index].Get(), std::format("Swap Chain Buffer {}", l_Index));

			m_Rtvs[l_Index] = m_RtvHeap->Allocate();
			if (!m_Rtvs[l_Index].IsValid())
			{
				return false;
			}

			D3D12_RENDER_TARGET_VIEW_DESC l_View = {};
			l_View.Format = k_RtvFormat;
			l_View.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			l_View.Texture2D.MipSlice = 0;
			l_View.Texture2D.PlaneSlice = 0;

			m_Device->GetHandle()->CreateRenderTargetView(m_Buffers[l_Index].Get(), &l_View, m_Rtvs[l_Index].CPU);
		}

		m_BufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

		return true;
	}

	void D3D12SwapChain::ReleaseBuffers()
	{
		for (uint32_t l_Index = 0; l_Index < k_BufferCount; ++l_Index)
		{
			if (m_RtvHeap != nullptr)
			{
				m_RtvHeap->Free(m_Rtvs[l_Index]);
			}

			m_Buffers[l_Index].Reset();
		}
	}

	UINT D3D12SwapChain::GetFlags() const
	{
		UINT l_Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		if (m_Tearing)
		{
			l_Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		}

		return l_Flags;
	}
}
#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"
#include "Powertrain/RHI/D3D12/D3D12DescriptorHeap.hpp"

#include <array>
#include <cstdint>

namespace Powertrain
{
	class D3D12Device;
	class D3D12CommandQueue;

	class D3D12SwapChain
	{
	public:
		static constexpr uint32_t k_BufferCount = 3;
		static constexpr DXGI_FORMAT k_Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		static constexpr DXGI_FORMAT k_RtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		D3D12SwapChain() = default;
		~D3D12SwapChain();

		D3D12SwapChain(const D3D12SwapChain&) = delete;
		D3D12SwapChain& operator=(const D3D12SwapChain&) = delete;

		bool Initialize(D3D12Device& device, D3D12CommandQueue& queue, D3D12DescriptorHeap& rtvHeap, HWND windowHandle, uint32_t width, uint32_t height, bool vsync);
		void Shutdown();

		// Blocks until the swap chain can accept another frame; call at the top of the frame, before input is sampled
		void WaitForNextFrame();

		// Both return false after a device removal, which has already been logged
		bool Present();
		bool Resize(uint32_t width, uint32_t height);

		void SetVSync(bool enabled) { m_VSync = enabled; }
		bool IsVSyncEnabled() const { return m_VSync; }
		bool IsTearingAllowed() const { return m_Tearing; }

		ID3D12Resource* GetCurrentBuffer() const { return m_Buffers[m_BufferIndex].Get(); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRtv() const { return m_Rtvs[m_BufferIndex].CPU; }
		uint32_t GetCurrentBufferIndex() const { return m_BufferIndex; }

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

	private:
		bool CreateBuffers();
		void ReleaseBuffers();
		UINT GetFlags() const;

	private:
		D3D12Device* m_Device = nullptr;
		D3D12CommandQueue* m_Queue = nullptr;
		D3D12DescriptorHeap* m_RtvHeap = nullptr;

		ComPtr<IDXGISwapChain4> m_SwapChain;
		HANDLE m_WaitableObject = nullptr;

		std::array<ComPtr<ID3D12Resource>, k_BufferCount> m_Buffers;
		std::array<DescriptorHandle, k_BufferCount> m_Rtvs;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		uint32_t m_BufferIndex = 0;

		bool m_VSync = true;
		bool m_Tearing = false;
	};
}
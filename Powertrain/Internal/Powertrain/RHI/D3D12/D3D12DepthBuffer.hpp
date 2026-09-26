#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"
#include "Powertrain/RHI/D3D12/D3D12DescriptorHeap.hpp"

#include <cstdint>

namespace Powertrain
{
	class D3D12Device;

	class D3D12DepthBuffer
	{
	public:
		static constexpr DXGI_FORMAT k_Format = DXGI_FORMAT_D32_FLOAT;
		static constexpr float k_ClearDepth = 0.0f;

		D3D12DepthBuffer() = default;
		~D3D12DepthBuffer();

		D3D12DepthBuffer(const D3D12DepthBuffer&) = delete;
		D3D12DepthBuffer& operator=(const D3D12DepthBuffer&) = delete;

		bool Initialize(D3D12Device& device, D3D12DescriptorHeap& dsvHeap, uint32_t width, uint32_t height, uint32_t sampleCount);
		void Shutdown();

		bool Resize(uint32_t width, uint32_t height);
		void Clear(ID3D12GraphicsCommandList* commandList) const;

		D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const { return m_Dsv.CPU; }
		ID3D12Resource* GetResource() const { return m_Resource.Get(); }
		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }
		uint32_t GetSampleCount() const { return m_SampleCount; }

	private:
		bool Create(uint32_t width, uint32_t height);
		void Release();

	private:
		D3D12Device* m_Device = nullptr;
		D3D12DescriptorHeap* m_DsvHeap = nullptr;

		ComPtr<ID3D12Resource> m_Resource;
		DescriptorHandle m_Dsv;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		uint32_t m_SampleCount = 1;
	};
}
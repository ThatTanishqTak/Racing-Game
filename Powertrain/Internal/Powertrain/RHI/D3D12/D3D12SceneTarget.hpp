#pragma once

#include "Powertrain/Renderer/RenderTypes.hpp"
#include "Powertrain/RHI/D3D12/D3D12Common.hpp"
#include "Powertrain/RHI/D3D12/D3D12DescriptorHeap.hpp"

#include <cstdint>

namespace Powertrain
{
	class D3D12Device;

	// The HDR scene colour: a multisampled RGBA16F target the scene passes draw into, and the single-sample copy it resolves into for the post pass
	class D3D12SceneTarget
	{
	public:
		static constexpr DXGI_FORMAT k_Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		static constexpr uint32_t k_PreferredSampleCount = 4;

		D3D12SceneTarget() = default;
		~D3D12SceneTarget();

		D3D12SceneTarget(const D3D12SceneTarget&) = delete;
		D3D12SceneTarget& operator=(const D3D12SceneTarget&) = delete;

		bool Initialize(D3D12Device& device, D3D12DescriptorHeap& rtvHeap, D3D12DescriptorHeap& resourceHeap, uint32_t width, uint32_t height, const Color& clearColor);
		void Shutdown();

		// Recreates the targets when the size or the optimized clear colour changed; call after the direct queue has been flushed
		bool Resize(uint32_t width, uint32_t height, const Color& clearColor);

		void Clear(ID3D12GraphicsCommandList* commandList) const;

		// Resolves the multisampled colour into the copy the post pass reads; both targets come back in their resting states
		void Resolve(ID3D12GraphicsCommandList* commandList);

		D3D12_CPU_DESCRIPTOR_HANDLE GetRtv() const { return m_Rtv.CPU; }
		uint32_t GetResolvedIndex() const { return m_ResolvedView.Index; }
		uint32_t GetSampleCount() const { return m_SampleCount; }
		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

	private:
		bool Create(uint32_t width, uint32_t height, const Color& clearColor);
		void Release();
		uint32_t QuerySampleCount() const;

	private:
		D3D12Device* m_Device = nullptr;
		D3D12DescriptorHeap* m_RtvHeap = nullptr;
		D3D12DescriptorHeap* m_ResourceHeap = nullptr;

		ComPtr<ID3D12Resource> m_Color;
		ComPtr<ID3D12Resource> m_Resolved;
		DescriptorHandle m_Rtv;
		DescriptorHandle m_ResolvedView;

		Color m_ClearColor;
		uint32_t m_SampleCount = 1;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		bool m_Initialized = false;
	};
}
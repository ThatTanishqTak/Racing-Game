#pragma once

#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/RHI/D3D12/D3D12Common.hpp"
#include "Powertrain/RHI/D3D12/D3D12DescriptorHeap.hpp"

#include <array>
#include <cstdint>

namespace Powertrain
{
	class D3D12Device;

	class D3D12ShadowMap
	{
	public:
		static constexpr uint32_t k_Size = 2048;
		static constexpr uint32_t k_CascadeCount = ShaderInterop::k_ShadowCascadeCount;
		static constexpr DXGI_FORMAT k_ResourceFormat = DXGI_FORMAT_R32_TYPELESS;
		static constexpr DXGI_FORMAT k_DepthFormat = DXGI_FORMAT_D32_FLOAT;
		static constexpr DXGI_FORMAT k_ShaderFormat = DXGI_FORMAT_R32_FLOAT;
		static constexpr float k_ClearDepth = 0.0f;

		D3D12ShadowMap() = default;
		~D3D12ShadowMap();

		D3D12ShadowMap(const D3D12ShadowMap&) = delete;
		D3D12ShadowMap& operator=(const D3D12ShadowMap&) = delete;

		bool Initialize(D3D12Device& device, D3D12DescriptorHeap& dsvHeap, D3D12DescriptorHeap& resourceHeap);
		void Shutdown();
		void TransitionTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state);
		void Clear(ID3D12GraphicsCommandList* commandList, uint32_t cascade) const;

		D3D12_CPU_DESCRIPTOR_HANDLE GetDsv(uint32_t cascade) const { return m_Dsvs[cascade].CPU; }
		uint32_t GetShaderResourceIndex() const { return m_ShaderResource.Index; }
		ID3D12Resource* GetResource() const { return m_Resource.Get(); }

	private:
		D3D12Device* m_Device = nullptr;
		D3D12DescriptorHeap* m_DsvHeap = nullptr;
		D3D12DescriptorHeap* m_ResourceHeap = nullptr;

		ComPtr<ID3D12Resource> m_Resource;
		std::array<DescriptorHandle, k_CascadeCount> m_Dsvs;
		DescriptorHandle m_ShaderResource;
		D3D12_RESOURCE_STATES m_State = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		bool m_Initialized = false;
	};
}
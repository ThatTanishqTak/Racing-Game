#pragma once

#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/RHI/D3D12/D3D12Common.hpp"
#include "Powertrain/RHI/D3D12/D3D12DescriptorHeap.hpp"

#include <array>
#include <cstdint>

namespace Powertrain
{
	class D3D12Device;

	class D3D12EnvironmentMaps
	{
	public:
		static constexpr DXGI_FORMAT k_Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		static constexpr DXGI_FORMAT k_BrdfFormat = DXGI_FORMAT_R16G16_FLOAT;
		static constexpr uint32_t k_FaceCount = ShaderInterop::k_CubeFaceCount;
		static constexpr uint32_t k_SpecularSize = 128;
		static constexpr uint32_t k_SpecularMipCount = 6;
		static constexpr uint32_t k_IrradianceSize = 32;
		static constexpr uint32_t k_BrdfSize = 256;

		static constexpr uint32_t GetSpecularMipSize(uint32_t mip) { return k_SpecularSize >> mip; }

		D3D12EnvironmentMaps() = default;
		~D3D12EnvironmentMaps();

		D3D12EnvironmentMaps(const D3D12EnvironmentMaps&) = delete;
		D3D12EnvironmentMaps& operator=(const D3D12EnvironmentMaps&) = delete;

		bool Initialize(D3D12Device& device, D3D12DescriptorHeap& rtvHeap, D3D12DescriptorHeap& resourceHeap);
		void Shutdown();
		void TransitionTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state);

		D3D12_CPU_DESCRIPTOR_HANDLE GetSpecularRtv(uint32_t face, uint32_t mip) const { return m_SpecularRtvs[mip * k_FaceCount + face].CPU; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetIrradianceRtv(uint32_t face) const { return m_IrradianceRtvs[face].CPU; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetBrdfRtv() const { return m_BrdfRtv.CPU; }

		uint32_t GetSpecularIndex() const { return m_SpecularView.Index; }
		uint32_t GetIrradianceIndex() const { return m_IrradianceView.Index; }
		uint32_t GetBrdfIndex() const { return m_BrdfView.Index; }

	private:
		bool CreateCube(ID3D12Resource** resource, uint32_t size, uint32_t mipCount, const char* name);

	private:
		D3D12Device* m_Device = nullptr;
		D3D12DescriptorHeap* m_RtvHeap = nullptr;
		D3D12DescriptorHeap* m_ResourceHeap = nullptr;

		ComPtr<ID3D12Resource> m_Specular;
		ComPtr<ID3D12Resource> m_Irradiance;
		ComPtr<ID3D12Resource> m_Brdf;

		std::array<DescriptorHandle, k_FaceCount* k_SpecularMipCount> m_SpecularRtvs;
		std::array<DescriptorHandle, k_FaceCount> m_IrradianceRtvs;
		DescriptorHandle m_BrdfRtv;

		DescriptorHandle m_SpecularView;
		DescriptorHandle m_IrradianceView;
		DescriptorHandle m_BrdfView;

		D3D12_RESOURCE_STATES m_State = D3D12_RESOURCE_STATE_RENDER_TARGET;

		bool m_Initialized = false;
	};
}
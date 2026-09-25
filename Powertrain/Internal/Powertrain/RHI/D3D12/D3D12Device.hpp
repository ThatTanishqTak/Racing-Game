#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <cstdint>
#include <string>

namespace Powertrain
{
	struct DeviceSettings
	{
		bool EnableDebugLayer = false;
		bool EnableGpuBasedValidation = false;
	};

	struct DeviceCapabilities
	{
		std::string AdapterName;
		uint64_t DedicatedVideoMemory = 0;

		D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;
		D3D_SHADER_MODEL ShaderModel = D3D_SHADER_MODEL_5_1;
		D3D12_RESOURCE_BINDING_TIER ResourceBindingTier = D3D12_RESOURCE_BINDING_TIER_1;

		bool EnhancedBarriers = false;
		bool Tearing = false;
		bool Bindless = false;
	};

	class D3D12Device
	{
	public:
		D3D12Device() = default;
		~D3D12Device();

		D3D12Device(const D3D12Device&) = delete;
		D3D12Device& operator=(const D3D12Device&) = delete;

		bool Initialize(const DeviceSettings& settings);
		void Shutdown();

		IDXGIFactory6* GetFactory() const { return m_Factory.Get(); }
		IDXGIAdapter4* GetAdapter() const { return m_Adapter.Get(); }
		ID3D12Device* GetHandle() const { return m_Device.Get(); }

		const DeviceCapabilities& GetCapabilities() const { return m_Capabilities; }
		bool IsDebugLayerEnabled() const { return m_DebugLayerEnabled; }

		// Logs the removal reason and, when DRED is on, the breadcrumbs and page fault that led to it
		void ReportDeviceRemoved() const;

	private:
		void EnableDebugLayer(const DeviceSettings& settings);
		bool CreateFactory();
		bool SelectAdapter();
		bool CreateDevice();
		void ConfigureInfoQueue();
		void QueryCapabilities();
		void ReportLiveObjects() const;

	private:
		ComPtr<IDXGIFactory6> m_Factory;
		ComPtr<IDXGIAdapter4> m_Adapter;
		ComPtr<ID3D12Device> m_Device;
		ComPtr<ID3D12InfoQueue> m_InfoQueue;

		DeviceCapabilities m_Capabilities;
		bool m_DebugLayerEnabled = false;
	};
}
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

#include "Powertrain/Core/CoreLog.hpp"

#include <dxgidebug.h>

#include <array>
#include <format>
#include <string>
#include <string_view>

namespace Powertrain
{
	namespace
	{
		// Creation floor; the targets below are checked and only warned about
		constexpr D3D_FEATURE_LEVEL k_MinimumFeatureLevel = D3D_FEATURE_LEVEL_11_0;
		constexpr D3D_FEATURE_LEVEL k_TargetFeatureLevel = D3D_FEATURE_LEVEL_12_1;
		constexpr D3D_SHADER_MODEL k_TargetShaderModel = D3D_SHADER_MODEL_6_6;
		constexpr D3D12_RESOURCE_BINDING_TIER k_TargetBindingTier = D3D12_RESOURCE_BINDING_TIER_3;

		std::string_view FeatureLevelName(D3D_FEATURE_LEVEL level)
		{
			switch (level)
			{
				case D3D_FEATURE_LEVEL_12_2:
				{
					return "12_2";
				}
				case D3D_FEATURE_LEVEL_12_1:
				{
					return "12_1";
				}
				case D3D_FEATURE_LEVEL_12_0:
				{
					return "12_0";
				}
				case D3D_FEATURE_LEVEL_11_1:
				{
					return "11_1";
				}
				case D3D_FEATURE_LEVEL_11_0:
				{
					return "11_0";
				}
				default:
				{
					return "?";
				}
			}
		}

		// D3D_SHADER_MODEL packs major and minor into hex nibbles
		std::string ShaderModelName(D3D_SHADER_MODEL model)
		{
			const int l_Value = static_cast<int>(model);

			return std::format("{}.{}", (l_Value >> 4) & 0xF, l_Value & 0xF);
		}

		std::string NameOrUnnamed(const wchar_t* name)
		{
			return name != nullptr ? Win32::ToNarrow(name) : std::string("unnamed");
		}
	}

	D3D12Device::~D3D12Device()
	{
		Shutdown();
	}

	bool D3D12Device::Initialize(const DeviceSettings& settings)
	{
		EnableDebugLayer(settings);

		if (!CreateFactory() || !SelectAdapter() || !CreateDevice())
		{
			return false;
		}

		ConfigureInfoQueue();
		QueryCapabilities();

		PT_CORE_INFO("Direct3D 12 device: {} ({} MB dedicated video memory)", m_Capabilities.AdapterName, m_Capabilities.DedicatedVideoMemory / (1024 * 1024));
		PT_CORE_INFO("Feature level {}, Shader Model {}, Resource Binding Tier {}, enhanced barriers {}, tearing {}", FeatureLevelName(m_Capabilities.FeatureLevel), ShaderModelName(m_Capabilities.ShaderModel), static_cast<int>(m_Capabilities.ResourceBindingTier), m_Capabilities.EnhancedBarriers ? "yes" : "no", m_Capabilities.Tearing ? "yes" : "no");

		if (!m_Capabilities.Bindless)
		{
			PT_CORE_WARN("Adapter falls short of feature level 12_1, Shader Model 6.6 and Resource Binding Tier 3; the bindless renderer (M6) needs all three");
		}

		return true;
	}

	void D3D12Device::Shutdown()
	{
		m_InfoQueue.Reset();
		m_Device.Reset();
		m_Adapter.Reset();
		m_Factory.Reset();

		if (m_DebugLayerEnabled)
		{
			ReportLiveObjects();
		}

		m_Capabilities = DeviceCapabilities();
		m_DebugLayerEnabled = false;
	}

	void D3D12Device::EnableDebugLayer(const DeviceSettings& settings)
	{
		if (!settings.EnableDebugLayer)
		{
			return;
		}

		ComPtr<ID3D12Debug> l_Debug;
		if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&l_Debug))))
		{
			PT_CORE_WARN("D3D12 debug layer requested but unavailable; install the 'Graphics Tools' optional Windows feature");

			return;
		}

		l_Debug->EnableDebugLayer();
		m_DebugLayerEnabled = true;

		if (settings.EnableGpuBasedValidation)
		{
			ComPtr<ID3D12Debug1> l_Debug1;
			if (SUCCEEDED(l_Debug.As(&l_Debug1)))
			{
				l_Debug1->SetEnableGPUBasedValidation(TRUE);
				l_Debug1->SetEnableSynchronizedCommandQueueValidation(TRUE);

				PT_CORE_INFO("GPU-based validation enabled");
			}
		}

		// DRED: breadcrumbs and page-fault data survive a device removal and are read back in ReportDeviceRemoved
		ComPtr<ID3D12DeviceRemovedExtendedDataSettings> l_DredSettings;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&l_DredSettings))))
		{
			l_DredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			l_DredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		}
	}

	bool D3D12Device::CreateFactory()
	{
		const UINT l_Flags = m_DebugLayerEnabled ? DXGI_CREATE_FACTORY_DEBUG : 0;
		if (!D3D12::CheckResult(CreateDXGIFactory2(l_Flags, IID_PPV_ARGS(&m_Factory)), "CreateDXGIFactory2"))
		{
			return false;
		}

		BOOL l_AllowTearing = FALSE;
		if (SUCCEEDED(m_Factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &l_AllowTearing, sizeof(l_AllowTearing))))
		{
			m_Capabilities.Tearing = l_AllowTearing == TRUE;
		}

		return true;
	}

	bool D3D12Device::SelectAdapter()
	{
		ComPtr<IDXGIAdapter4> l_Adapter;
		for (UINT l_Index = 0; m_Factory->EnumAdapterByGpuPreference(l_Index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&l_Adapter)) != DXGI_ERROR_NOT_FOUND; ++l_Index)
		{
			DXGI_ADAPTER_DESC3 l_Description = {};
			l_Adapter->GetDesc3(&l_Description);

			if (l_Description.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)
			{
				continue;
			}

			// Probe only (null output); the device is created once, after the adapter is chosen
			if (SUCCEEDED(D3D12CreateDevice(l_Adapter.Get(), k_MinimumFeatureLevel, __uuidof(ID3D12Device), nullptr)))
			{
				m_Adapter = l_Adapter;
				m_Capabilities.AdapterName = Win32::ToNarrow(l_Description.Description);
				m_Capabilities.DedicatedVideoMemory = l_Description.DedicatedVideoMemory;

				return true;
			}
		}

		PT_CORE_ERROR("No hardware adapter supports Direct3D 12 feature level 11_0");

		return false;
	}

	bool D3D12Device::CreateDevice()
	{
		if (!D3D12::CheckResult(D3D12CreateDevice(m_Adapter.Get(), k_MinimumFeatureLevel, IID_PPV_ARGS(&m_Device)), "D3D12CreateDevice"))
		{
			return false;
		}

		D3D12::SetDebugName(m_Device.Get(), "D3D12Device");

		return true;
	}

	void D3D12Device::ConfigureInfoQueue()
	{
		if (!m_DebugLayerEnabled || FAILED(m_Device.As(&m_InfoQueue)))
		{
			return;
		}

		// Break in the debugger on the message itself, with the offending call still on the stack
		m_InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		m_InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

		// Info-level messages are noise in the debugger output
		std::array<D3D12_MESSAGE_SEVERITY, 1> l_Severities = { D3D12_MESSAGE_SEVERITY_INFO };

		D3D12_INFO_QUEUE_FILTER l_Filter = {};
		l_Filter.DenyList.NumSeverities = static_cast<UINT>(l_Severities.size());
		l_Filter.DenyList.pSeverityList = l_Severities.data();
		m_InfoQueue->PushStorageFilter(&l_Filter);
	}

	void D3D12Device::QueryCapabilities()
	{
		constexpr std::array<D3D_FEATURE_LEVEL, 5> l_Levels = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

		D3D12_FEATURE_DATA_FEATURE_LEVELS l_FeatureLevels = {};
		l_FeatureLevels.NumFeatureLevels = static_cast<UINT>(l_Levels.size());
		l_FeatureLevels.pFeatureLevelsRequested = l_Levels.data();
		if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &l_FeatureLevels, sizeof(l_FeatureLevels))))
		{
			m_Capabilities.FeatureLevel = l_FeatureLevels.MaxSupportedFeatureLevel;
		}

		// The runtime rejects a shader model it does not know with E_INVALIDARG, so ask from the newest down
		constexpr std::array<D3D_SHADER_MODEL, 3> l_Models = { D3D_SHADER_MODEL_6_6, D3D_SHADER_MODEL_6_5, D3D_SHADER_MODEL_6_0 };
		for (D3D_SHADER_MODEL l_Model : l_Models)
		{
			D3D12_FEATURE_DATA_SHADER_MODEL l_ShaderModel = { l_Model };
			if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &l_ShaderModel, sizeof(l_ShaderModel))))
			{
				m_Capabilities.ShaderModel = l_ShaderModel.HighestShaderModel;

				break;
			}
		}

		D3D12_FEATURE_DATA_D3D12_OPTIONS l_Options = {};
		if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &l_Options, sizeof(l_Options))))
		{
			m_Capabilities.ResourceBindingTier = l_Options.ResourceBindingTier;
		}

#ifdef __ID3D12Device10_INTERFACE_DEFINED__
		// OPTIONS12 arrived in the same SDK as ID3D12Device10 (10.0.22621); older SDKs simply report no enhanced barriers
		D3D12_FEATURE_DATA_D3D12_OPTIONS12 l_Options12 = {};
		if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &l_Options12, sizeof(l_Options12))))
		{
			m_Capabilities.EnhancedBarriers = l_Options12.EnhancedBarriersSupported == TRUE;
		}
#endif

		m_Capabilities.Bindless = m_Capabilities.FeatureLevel >= k_TargetFeatureLevel && m_Capabilities.ShaderModel >= k_TargetShaderModel && m_Capabilities.ResourceBindingTier >= k_TargetBindingTier;
	}

	void D3D12Device::ReportLiveObjects() const
	{
		ComPtr<IDXGIDebug1> l_Debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&l_Debug))))
		{
			// Anything listed in the debugger output outlived the renderer; the goal is an empty report
			l_Debug->ReportLiveObjects(DXGI_DEBUG_ALL, static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		}
	}

	void D3D12Device::ReportDeviceRemoved() const
	{
		if (!m_Device)
		{
			return;
		}

		PT_CORE_ERROR("D3D12Device removed: {}", D3D12::ResultToString(m_Device->GetDeviceRemovedReason()));

		ComPtr<ID3D12DeviceRemovedExtendedData> l_Dred;
		if (FAILED(m_Device.As(&l_Dred)))
		{
			return;
		}

		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT l_Breadcrumbs = {};
		if (SUCCEEDED(l_Dred->GetAutoBreadcrumbsOutput(&l_Breadcrumbs)))
		{
			for (const D3D12_AUTO_BREADCRUMB_NODE* l_Node = l_Breadcrumbs.pHeadAutoBreadcrumbNode; l_Node != nullptr; l_Node = l_Node->pNext)
			{
				const UINT l_Completed = l_Node->pLastBreadcrumbValue != nullptr ? *l_Node->pLastBreadcrumbValue : 0;
				if (l_Completed == l_Node->BreadcrumbCount)
				{
					// This list ran to completion; the fault is in another one
					continue;
				}

				const int l_Operation = l_Completed < l_Node->BreadcrumbCount ? static_cast<int>(l_Node->pCommandHistory[l_Completed]) : -1;
				PT_CORE_ERROR("  Breadcrumb: list '{}' on queue '{}' stopped at op {} of {} (D3D12_AUTO_BREADCRUMB_OP {})", NameOrUnnamed(l_Node->pCommandListDebugNameW), NameOrUnnamed(l_Node->pCommandQueueDebugNameW), l_Completed, l_Node->BreadcrumbCount, l_Operation);
			}
		}

		D3D12_DRED_PAGE_FAULT_OUTPUT l_PageFault = {};
		if (SUCCEEDED(l_Dred->GetPageFaultAllocationOutput(&l_PageFault)))
		{
			PT_CORE_ERROR("  Page fault at GPU virtual address 0x{:016X}", l_PageFault.PageFaultVA);

			for (const D3D12_DRED_ALLOCATION_NODE* l_Node = l_PageFault.pHeadExistingAllocationNode; l_Node != nullptr; l_Node = l_Node->pNext)
			{
				PT_CORE_ERROR("    Existing allocation '{}' (type {})", NameOrUnnamed(l_Node->ObjectNameW), static_cast<int>(l_Node->AllocationType));
			}

			for (const D3D12_DRED_ALLOCATION_NODE* l_Node = l_PageFault.pHeadRecentFreedAllocationNode; l_Node != nullptr; l_Node = l_Node->pNext)
			{
				PT_CORE_ERROR("    Recently freed allocation '{}' (type {})", NameOrUnnamed(l_Node->ObjectNameW), static_cast<int>(l_Node->AllocationType));
			}
		}
	}
}
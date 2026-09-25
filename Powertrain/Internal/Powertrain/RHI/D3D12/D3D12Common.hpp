#pragma once

#include "Powertrain/Platform/Windows/Win32.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace Powertrain
{
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	namespace D3D12
	{
		constexpr uint32_t k_FramesInFlight = 2;

		std::string ResultToString(HRESULT result);
		
		bool CheckResult(HRESULT result, std::string_view what);
		
		void SetDebugName(ID3D12Object* object, std::string_view name);
	}
}
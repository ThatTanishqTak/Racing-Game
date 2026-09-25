#include "Powertrain/RHI/D3D12/D3D12CommandList.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

#include <format>

namespace Powertrain
{
	D3D12CommandList::~D3D12CommandList()
	{
		Shutdown();
	}

	bool D3D12CommandList::Initialize(D3D12Device& device, D3D12_COMMAND_LIST_TYPE type, std::string_view name)
	{
		ID3D12Device* l_Device = device.GetHandle();

		for (uint32_t l_Index = 0; l_Index < D3D12::k_FramesInFlight; ++l_Index)
		{
			if (!D3D12::CheckResult(l_Device->CreateCommandAllocator(type, IID_PPV_ARGS(&m_Allocators[l_Index])), std::format("CreateCommandAllocator '{}' {}", name, l_Index)))
			{
				return false;
			}

			D3D12::SetDebugName(m_Allocators[l_Index].Get(), std::format("{} Allocator {}", name, l_Index));
		}

		if (!D3D12::CheckResult(l_Device->CreateCommandList(0, type, m_Allocators[0].Get(), nullptr, IID_PPV_ARGS(&m_CommandList)), std::format("CreateCommandList '{}'", name)))
		{
			return false;
		}

		// Lists are created open; close it so every frame, the first included, starts with Reset
		m_CommandList->Close();
		m_Recording = false;

		D3D12::SetDebugName(m_CommandList.Get(), name);

		return true;
	}

	void D3D12CommandList::Shutdown()
	{
		if (m_Recording && m_CommandList)
		{
			m_CommandList->Close();
		}

		m_Recording = false;
		m_CommandList.Reset();

		for (ComPtr<ID3D12CommandAllocator>& l_Allocator : m_Allocators)
		{
			l_Allocator.Reset();
		}
	}

	bool D3D12CommandList::Reset(uint32_t frameIndex)
	{
		PT_CORE_ASSERT(frameIndex < D3D12::k_FramesInFlight, "Frame index {} out of range", frameIndex);
		PT_CORE_ASSERT(!m_Recording, "D3D12CommandList reset while still recording");

		ID3D12CommandAllocator* l_Allocator = m_Allocators[frameIndex].Get();
		if (!D3D12::CheckResult(l_Allocator->Reset(), "ID3D12CommandAllocator::Reset"))
		{
			return false;
		}

		if (!D3D12::CheckResult(m_CommandList->Reset(l_Allocator, nullptr), "ID3D12GraphicsCommandList::Reset"))
		{
			return false;
		}

		m_Recording = true;

		return true;
	}

	bool D3D12CommandList::Close()
	{
		PT_CORE_ASSERT(m_Recording, "D3D12CommandList closed without being reset");
		m_Recording = false;

		return D3D12::CheckResult(m_CommandList->Close(), "ID3D12GraphicsCommandList::Close");
	}
}
#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace Powertrain
{
	class D3D12Device;

	class D3D12CommandList
	{
	public:
		D3D12CommandList() = default;
		~D3D12CommandList();

		D3D12CommandList(const D3D12CommandList&) = delete;
		D3D12CommandList& operator=(const D3D12CommandList&) = delete;

		bool Initialize(D3D12Device& device, D3D12_COMMAND_LIST_TYPE type, std::string_view name);
		void Shutdown();

		bool Reset(uint32_t frameIndex);
		bool Close();

		ID3D12GraphicsCommandList* GetHandle() const { return m_CommandList.Get(); }
		bool IsRecording() const { return m_Recording; }

	private:
		std::array<ComPtr<ID3D12CommandAllocator>, D3D12::k_FramesInFlight> m_Allocators;
		ComPtr<ID3D12GraphicsCommandList> m_CommandList;

		bool m_Recording = false;
	};
}
#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <cstdint>
#include <string_view>

namespace Powertrain
{
	class D3D12Device;

	class D3D12CommandQueue
	{
	public:
		D3D12CommandQueue() = default;
		~D3D12CommandQueue();

		D3D12CommandQueue(const D3D12CommandQueue&) = delete;
		D3D12CommandQueue& operator=(const D3D12CommandQueue&) = delete;

		bool Initialize(D3D12Device& device, D3D12_COMMAND_LIST_TYPE type, std::string_view name);
		void Shutdown();

		// Submits the list and signals the fence; the returned value completes once the GPU has finished it
		uint64_t ExecuteCommandList(ID3D12CommandList* commandList);

		uint64_t Signal();
		bool IsFenceComplete(uint64_t value) const;
		void WaitForFence(uint64_t value);

		// Blocks until everything submitted so far has finished; used on resize and shutdown
		void Flush();

		ID3D12CommandQueue* GetHandle() const { return m_Queue.Get(); }
		ID3D12Fence* GetFence() const { return m_Fence.Get(); }
		D3D12_COMMAND_LIST_TYPE GetType() const { return m_Type; }
		uint64_t GetLastSignaledValue() const { return m_LastSignaledValue; }
		uint64_t GetCompletedValue() const;

	private:
		ComPtr<ID3D12CommandQueue> m_Queue;
		ComPtr<ID3D12Fence> m_Fence;
		HANDLE m_FenceEvent = nullptr;

		D3D12_COMMAND_LIST_TYPE m_Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		uint64_t m_LastSignaledValue = 0;
	};
}
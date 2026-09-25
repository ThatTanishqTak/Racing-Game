#include "Powertrain/RHI/D3D12/D3D12CommandQueue.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

#include <format>

namespace Powertrain
{
	D3D12CommandQueue::~D3D12CommandQueue()
	{
		Shutdown();
	}

	bool D3D12CommandQueue::Initialize(D3D12Device& device, D3D12_COMMAND_LIST_TYPE type, std::string_view name)
	{
		m_Type = type;
		m_LastSignaledValue = 0;

		D3D12_COMMAND_QUEUE_DESC l_Description = {};
		l_Description.Type = type;
		l_Description.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		l_Description.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

		if (!D3D12::CheckResult(device.GetHandle()->CreateCommandQueue(&l_Description, IID_PPV_ARGS(&m_Queue)), std::format("CreateCommandQueue '{}'", name)))
		{
			return false;
		}

		if (!D3D12::CheckResult(device.GetHandle()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)), std::format("CreateFence '{}'", name)))
		{
			return false;
		}

		m_FenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (m_FenceEvent == nullptr)
		{
			PT_CORE_ERROR("CreateEventW failed with error {}", GetLastError());

			return false;
		}

		D3D12::SetDebugName(m_Queue.Get(), name);
		D3D12::SetDebugName(m_Fence.Get(), std::format("{} Fence", name));

		return true;
	}

	void D3D12CommandQueue::Shutdown()
	{
		if (m_Queue && m_Fence)
		{
			Flush();
		}

		if (m_FenceEvent != nullptr)
		{
			CloseHandle(m_FenceEvent);
			m_FenceEvent = nullptr;
		}

		m_Fence.Reset();
		m_Queue.Reset();
		m_LastSignaledValue = 0;
	}

	uint64_t D3D12CommandQueue::ExecuteCommandList(ID3D12CommandList* commandList)
	{
		ID3D12CommandList* l_Lists[] = { commandList };
		m_Queue->ExecuteCommandLists(1, l_Lists);

		return Signal();
	}

	uint64_t D3D12CommandQueue::Signal()
	{
		++m_LastSignaledValue;
		D3D12::CheckResult(m_Queue->Signal(m_Fence.Get(), m_LastSignaledValue), "ID3D12CommandQueue::Signal");

		return m_LastSignaledValue;
	}

	bool D3D12CommandQueue::IsFenceComplete(uint64_t value) const
	{
		// After a device removal GetCompletedValue returns UINT64_MAX, so waits never hang
		return m_Fence->GetCompletedValue() >= value;
	}

	void D3D12CommandQueue::WaitForFence(uint64_t value)
	{
		if (IsFenceComplete(value))
		{
			return;
		}

		if (D3D12::CheckResult(m_Fence->SetEventOnCompletion(value, m_FenceEvent), "ID3D12Fence::SetEventOnCompletion"))
		{
			WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
		}
	}

	void D3D12CommandQueue::Flush()
	{
		WaitForFence(Signal());
	}

	uint64_t D3D12CommandQueue::GetCompletedValue() const
	{
		return m_Fence->GetCompletedValue();
	}
}
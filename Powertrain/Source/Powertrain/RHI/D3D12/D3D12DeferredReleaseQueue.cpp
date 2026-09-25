#include "Powertrain/RHI/D3D12/D3D12DeferredReleaseQueue.hpp"

#include "Powertrain/Core/CoreLog.hpp"

#include <algorithm>
#include <utility>

namespace Powertrain
{
	D3D12DeferredReleaseQueue::~D3D12DeferredReleaseQueue()
	{
		if (!m_Entries.empty())
		{
			PT_CORE_WARN("Deferred release queue destroyed with {} entries pending; releasing them now", m_Entries.size());
		}
	}

	void D3D12DeferredReleaseQueue::Release(uint64_t completedFenceValue)
	{
		const auto l_NewEnd = std::remove_if(m_Entries.begin(), m_Entries.end(), [completedFenceValue](const Entry& entry)
		{
			return entry.FenceValue <= completedFenceValue;
		});

		m_Entries.erase(l_NewEnd, m_Entries.end());
	}

	void D3D12DeferredReleaseQueue::Flush()
	{
		m_Entries.clear();
	}

	void D3D12DeferredReleaseQueue::Push(ComPtr<IUnknown> resource, uint64_t fenceValue)
	{
		m_Entries.push_back({ std::move(resource), fenceValue });
	}
}
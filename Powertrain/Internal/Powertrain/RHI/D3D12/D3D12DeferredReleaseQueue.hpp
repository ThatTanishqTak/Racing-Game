#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Powertrain
{
	class D3D12DeferredReleaseQueue
	{
	public:
		D3D12DeferredReleaseQueue() = default;
		~D3D12DeferredReleaseQueue();

		D3D12DeferredReleaseQueue(const D3D12DeferredReleaseQueue&) = delete;
		D3D12DeferredReleaseQueue& operator=(const D3D12DeferredReleaseQueue&) = delete;

		// Takes ownership and clears the caller's pointer; a null pointer is ignored
		template<typename T>
		void Enqueue(ComPtr<T>& resource, uint64_t fenceValue)
		{
			if (resource)
			{
				Push(ComPtr<IUnknown>(resource), fenceValue);
				resource.Reset();
			}
		}

		// Releases every entry whose fence value is at or below the completed value
		void Release(uint64_t completedFenceValue);

		// Releases everything
		void Flush();

		size_t GetPendingCount() const { return m_Entries.size(); }

	private:
		struct Entry
		{
			ComPtr<IUnknown> Resource;
			uint64_t FenceValue = 0;
		};

		void Push(ComPtr<IUnknown> resource, uint64_t fenceValue);

	private:
		std::vector<Entry> m_Entries;
	};
}
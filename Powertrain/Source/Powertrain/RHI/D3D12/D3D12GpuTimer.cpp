#include "Powertrain/RHI/D3D12/D3D12GpuTimer.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12CommandQueue.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

#include <cstring>
#include <format>

namespace Powertrain
{
	namespace
	{
		constexpr uint32_t k_QueriesPerFrame = D3D12GpuTimer::k_MaxTimers * 2;
		constexpr uint32_t k_QueryCount = k_QueriesPerFrame * D3D12::k_FramesInFlight;
		constexpr uint64_t k_ReadbackBytes = static_cast<uint64_t>(k_QueryCount) * sizeof(uint64_t);
	}

	D3D12GpuTimer::~D3D12GpuTimer()
	{
		Shutdown();
	}

	bool D3D12GpuTimer::Initialize(D3D12Device& device, D3D12CommandQueue& queue, std::string_view name)
	{
		PT_CORE_ASSERT(queue.GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT, "GPU timer needs the direct queue");

		ID3D12Device* l_Device = device.GetHandle();

		D3D12_QUERY_HEAP_DESC l_HeapDescription = {};
		l_HeapDescription.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		l_HeapDescription.Count = k_QueryCount;
		l_HeapDescription.NodeMask = 0;

		if (!D3D12::CheckResult(l_Device->CreateQueryHeap(&l_HeapDescription, IID_PPV_ARGS(&m_QueryHeap)), std::format("CreateQueryHeap '{}'", name)))
		{
			return false;
		}

		D3D12::SetDebugName(m_QueryHeap.Get(), std::format("{} Query Heap", name));

		D3D12_HEAP_PROPERTIES l_HeapProperties = {};
		l_HeapProperties.Type = D3D12_HEAP_TYPE_READBACK;

		D3D12_RESOURCE_DESC l_BufferDescription = {};
		l_BufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		l_BufferDescription.Width = k_ReadbackBytes;
		l_BufferDescription.Height = 1;
		l_BufferDescription.DepthOrArraySize = 1;
		l_BufferDescription.MipLevels = 1;
		l_BufferDescription.Format = DXGI_FORMAT_UNKNOWN;
		l_BufferDescription.SampleDesc = { 1, 0 };
		l_BufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		l_BufferDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

		if (!D3D12::CheckResult(l_Device->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_BufferDescription, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_ReadbackBuffer)), std::format("CreateCommittedResource '{}'", name)))
		{
			return false;
		}

		D3D12::SetDebugName(m_ReadbackBuffer.Get(), std::format("{} Readback", name));

		// A queue that cannot report its frequency leaves the timer in place but reporting zeros
		if (!D3D12::CheckResult(queue.GetHandle()->GetTimestampFrequency(&m_Frequency), "ID3D12CommandQueue::GetTimestampFrequency"))
		{
			m_Frequency = 0;
		}

		for (std::array<bool, k_MaxTimers>& l_Frame : m_Used)
		{
			l_Frame.fill(false);
		}

		for (std::array<bool, k_MaxTimers>& l_Frame : m_Ended)
		{
			l_Frame.fill(false);
		}

		m_Results.fill(0.0);
		m_FrameIndex = 0;

		PT_CORE_INFO("GPU timer ready: {} slots, {} MHz timestamps", k_MaxTimers, m_Frequency / 1000000);

		return true;
	}

	void D3D12GpuTimer::Shutdown()
	{
		m_ReadbackBuffer.Reset();
		m_QueryHeap.Reset();
		m_Frequency = 0;
		m_FrameIndex = 0;
		m_Results.fill(0.0);
	}

	void D3D12GpuTimer::BeginFrame(uint32_t frameIndex)
	{
		PT_CORE_ASSERT(frameIndex < D3D12::k_FramesInFlight, "Frame index {} out of range", frameIndex);

		if (!m_QueryHeap)
		{
			return;
		}

		ReadBack(frameIndex);

		m_FrameIndex = frameIndex;
		m_Used[frameIndex].fill(false);
		m_Ended[frameIndex].fill(false);
	}

	void D3D12GpuTimer::Begin(ID3D12GraphicsCommandList* commandList, uint32_t timer)
	{
		PT_CORE_ASSERT(timer < k_MaxTimers, "GPU timer {} out of range", timer);
		PT_CORE_ASSERT(!m_Used[m_FrameIndex][timer], "GPU timer {} begun twice in one frame", timer);

		if (!m_QueryHeap)
		{
			return;
		}

		m_Used[m_FrameIndex][timer] = true;

		// Timestamps have no BeginQuery; a pair of EndQuery calls brackets the work
		commandList->EndQuery(m_QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, QueryIndex(m_FrameIndex, timer));
	}

	void D3D12GpuTimer::End(ID3D12GraphicsCommandList* commandList, uint32_t timer)
	{
		PT_CORE_ASSERT(timer < k_MaxTimers, "GPU timer {} out of range", timer);
		PT_CORE_ASSERT(m_Used[m_FrameIndex][timer], "GPU timer {} ended without Begin", timer);

		if (!m_QueryHeap)
		{
			return;
		}

		m_Ended[m_FrameIndex][timer] = true;

		commandList->EndQuery(m_QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, QueryIndex(m_FrameIndex, timer) + 1);
	}

	void D3D12GpuTimer::Resolve(ID3D12GraphicsCommandList* commandList)
	{
		if (!m_QueryHeap)
		{
			return;
		}

		// Only pairs that were actually written are resolved; the debug layer flags a resolve of an unissued query
		for (uint32_t l_Timer = 0; l_Timer < k_MaxTimers; ++l_Timer)
		{
			if (!m_Used[m_FrameIndex][l_Timer] || !m_Ended[m_FrameIndex][l_Timer])
			{
				continue;
			}

			const uint32_t l_QueryIndex = QueryIndex(m_FrameIndex, l_Timer);
			commandList->ResolveQueryData(m_QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, l_QueryIndex, 2, m_ReadbackBuffer.Get(), static_cast<uint64_t>(l_QueryIndex) * sizeof(uint64_t));
		}
	}

	double D3D12GpuTimer::GetMilliseconds(uint32_t timer) const
	{
		PT_CORE_ASSERT(timer < k_MaxTimers, "GPU timer {} out of range", timer);

		return m_Results[timer];
	}

	void D3D12GpuTimer::ReadBack(uint32_t frameIndex)
	{
		m_Results.fill(0.0);

		if (m_Frequency == 0)
		{
			return;
		}

		bool l_AnyUsed = false;
		for (uint32_t l_Timer = 0; l_Timer < k_MaxTimers; ++l_Timer)
		{
			l_AnyUsed = l_AnyUsed || (m_Used[frameIndex][l_Timer] && m_Ended[frameIndex][l_Timer]);
		}

		if (!l_AnyUsed)
		{
			return;
		}

		// Map only this frame's region; the fence wait in the renderer guarantees the GPU has finished writing it
		const uint64_t l_FirstQuery = QueryIndex(frameIndex, 0);
		const D3D12_RANGE l_ReadRange = { static_cast<SIZE_T>(l_FirstQuery * sizeof(uint64_t)), static_cast<SIZE_T>((l_FirstQuery + k_QueriesPerFrame) * sizeof(uint64_t)) };
		const D3D12_RANGE l_NoWrite = { 0, 0 };

		void* l_Mapped = nullptr;
		if (!D3D12::CheckResult(m_ReadbackBuffer->Map(0, &l_ReadRange, &l_Mapped), "ID3D12Resource::Map (GPU timer readback)"))
		{
			return;
		}

		const uint64_t* l_Timestamps = static_cast<const uint64_t*>(l_Mapped) + l_FirstQuery;
		const double l_TicksToMilliseconds = 1000.0 / static_cast<double>(m_Frequency);

		for (uint32_t l_Timer = 0; l_Timer < k_MaxTimers; ++l_Timer)
		{
			if (!m_Used[frameIndex][l_Timer] || !m_Ended[frameIndex][l_Timer])
			{
				continue;
			}

			const uint64_t l_Start = l_Timestamps[l_Timer * 2];
			const uint64_t l_End = l_Timestamps[l_Timer * 2 + 1];

			// A disjoint pair (clock reset mid-frame) reads as zero rather than a huge number
			m_Results[l_Timer] = l_End >= l_Start ? static_cast<double>(l_End - l_Start) * l_TicksToMilliseconds : 0.0;
		}

		m_ReadbackBuffer->Unmap(0, &l_NoWrite);
	}
}
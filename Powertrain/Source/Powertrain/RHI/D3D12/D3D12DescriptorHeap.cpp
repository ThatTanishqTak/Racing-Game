#include "Powertrain/RHI/D3D12/D3D12DescriptorHeap.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

#include <format>

namespace Powertrain
{
	D3D12DescriptorHeap::~D3D12DescriptorHeap()
	{
		Shutdown();
	}

	bool D3D12DescriptorHeap::Initialize(D3D12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t persistentCount, uint32_t transientCountPerFrame, bool shaderVisible, std::string_view name)
	{
		PT_CORE_ASSERT(persistentCount + transientCountPerFrame > 0, "Descriptor heap '{}' has no descriptors", name);

		const bool l_CanBeShaderVisible = type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		if (shaderVisible && !l_CanBeShaderVisible)
		{
			PT_CORE_ERROR("Descriptor heap '{}': RTV and DSV heaps cannot be shader visible", name);

			return false;
		}

		m_Type = type;
		m_PersistentCount = persistentCount;
		m_TransientCountPerFrame = transientCountPerFrame;
		m_PersistentHighWater = 0;
		m_PersistentInUse = 0;
		m_TransientInUse = 0;
		m_FrameIndex = 0;
		m_FreeList.clear();

		D3D12_DESCRIPTOR_HEAP_DESC l_Description = {};
		l_Description.Type = type;
		l_Description.NumDescriptors = persistentCount + transientCountPerFrame * D3D12::k_FramesInFlight;
		l_Description.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		l_Description.NodeMask = 0;

		if (!D3D12::CheckResult(device.GetHandle()->CreateDescriptorHeap(&l_Description, IID_PPV_ARGS(&m_Heap)), std::format("CreateDescriptorHeap '{}'", name)))
		{
			return false;
		}

		D3D12::SetDebugName(m_Heap.Get(), name);

		m_DescriptorSize = device.GetHandle()->GetDescriptorHandleIncrementSize(type);
		m_CpuStart = m_Heap->GetCPUDescriptorHandleForHeapStart();
		m_GpuStart = shaderVisible ? m_Heap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{};

		return true;
	}

	void D3D12DescriptorHeap::Shutdown()
	{
		if (m_Heap && m_PersistentInUse != 0)
		{
			PT_CORE_WARN("Descriptor heap shut down with {} persistent descriptors still allocated", m_PersistentInUse);
		}

		m_Heap.Reset();
		m_FreeList.clear();
		m_CpuStart = {};
		m_GpuStart = {};
		m_DescriptorSize = 0;
		m_PersistentCount = 0;
		m_PersistentHighWater = 0;
		m_PersistentInUse = 0;
		m_TransientCountPerFrame = 0;
		m_TransientInUse = 0;
		m_FrameIndex = 0;
	}

	DescriptorHandle D3D12DescriptorHeap::Allocate()
	{
		uint32_t l_Index = UINT32_MAX;

		if (!m_FreeList.empty())
		{
			l_Index = m_FreeList.back();
			m_FreeList.pop_back();
		}
		else if (m_PersistentHighWater < m_PersistentCount)
		{
			l_Index = m_PersistentHighWater++;
		}
		else
		{
			PT_CORE_ERROR("Descriptor heap out of persistent descriptors ({} in use)", m_PersistentCount);

			return DescriptorHandle();
		}

		++m_PersistentInUse;

		return MakeHandle(l_Index);
	}

	void D3D12DescriptorHeap::Free(DescriptorHandle& handle)
	{
		if (!handle.IsValid())
		{
			return;
		}

		PT_CORE_ASSERT(handle.Index < m_PersistentCount, "Descriptor {} is not a persistent descriptor of this heap", handle.Index);

		m_FreeList.push_back(handle.Index);
		--m_PersistentInUse;

		handle = DescriptorHandle();
	}

	void D3D12DescriptorHeap::BeginFrame(uint32_t frameIndex)
	{
		PT_CORE_ASSERT(frameIndex < D3D12::k_FramesInFlight, "Frame index {} out of range", frameIndex);

		m_FrameIndex = frameIndex;
		m_TransientInUse = 0;
	}

	DescriptorHandle D3D12DescriptorHeap::AllocateTransient(uint32_t count)
	{
		if (count == 0 || m_TransientInUse + count > m_TransientCountPerFrame)
		{
			PT_CORE_ERROR("Descriptor heap out of transient descriptors ({} of {} used, {} requested)", m_TransientInUse, m_TransientCountPerFrame, count);

			return DescriptorHandle();
		}

		const uint32_t l_Index = m_PersistentCount + m_FrameIndex * m_TransientCountPerFrame + m_TransientInUse;
		m_TransientInUse += count;

		return MakeHandle(l_Index);
	}

	DescriptorHandle D3D12DescriptorHeap::MakeHandle(uint32_t index) const
	{
		const SIZE_T l_Offset = static_cast<SIZE_T>(index) * m_DescriptorSize;

		DescriptorHandle l_Handle;
		l_Handle.Index = index;
		l_Handle.CPU.ptr = m_CpuStart.ptr + l_Offset;
		l_Handle.GPU.ptr = m_GpuStart.ptr != 0 ? m_GpuStart.ptr + l_Offset : 0;

		return l_Handle;
	}
}
#include "Powertrain/RHI/D3D12/D3D12UploadRing.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

#include <algorithm>
#include <format>

namespace Powertrain
{
	D3D12UploadRing::~D3D12UploadRing()
	{
		Shutdown();
	}

	bool D3D12UploadRing::Initialize(D3D12Device& device, uint64_t bytesPerFrame, std::string_view name)
	{
		PT_CORE_ASSERT(bytesPerFrame > 0 && bytesPerFrame % k_ConstantAlignment == 0, "Upload ring '{}' needs a positive multiple of {} bytes per frame, got {}", name, k_ConstantAlignment, bytesPerFrame);

		m_BytesPerFrame = bytesPerFrame;
		m_Offset = 0;
		m_HighWater = 0;
		m_FrameIndex = 0;
		m_OverflowWarned = false;

		D3D12_HEAP_PROPERTIES l_HeapProperties = {};
		l_HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC l_BufferDescription = {};
		l_BufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		l_BufferDescription.Width = bytesPerFrame;
		l_BufferDescription.Height = 1;
		l_BufferDescription.DepthOrArraySize = 1;
		l_BufferDescription.MipLevels = 1;
		l_BufferDescription.Format = DXGI_FORMAT_UNKNOWN;
		l_BufferDescription.SampleDesc = { 1, 0 };
		l_BufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		l_BufferDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

		for (uint32_t l_Index = 0; l_Index < D3D12::k_FramesInFlight; ++l_Index)
		{
			FrameBuffer& l_Frame = m_Frames[l_Index];

			if (!D3D12::CheckResult(device.GetHandle()->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_BufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&l_Frame.Buffer)), std::format("CreateCommittedResource ('{}' {})", name, l_Index)))
			{
				Shutdown();

				return false;
			}

			D3D12::SetDebugName(l_Frame.Buffer.Get(), std::format("{} {}", name, l_Index));

			// Upload heaps stay mapped for their whole life; the CPU never reads them back
			const D3D12_RANGE l_NoRead = { 0, 0 };
			void* l_Mapped = nullptr;
			if (!D3D12::CheckResult(l_Frame.Buffer->Map(0, &l_NoRead, &l_Mapped), std::format("ID3D12Resource::Map ('{}' {})", name, l_Index)))
			{
				Shutdown();

				return false;
			}

			l_Frame.Mapped = static_cast<uint8_t*>(l_Mapped);
			l_Frame.GpuAddress = l_Frame.Buffer->GetGPUVirtualAddress();
		}

		m_Initialized = true;

		PT_CORE_INFO("Upload ring ready: {} MB per frame", bytesPerFrame / (1024 * 1024));

		return true;
	}

	void D3D12UploadRing::Shutdown()
	{
		for (FrameBuffer& l_Frame : m_Frames)
		{
			if (l_Frame.Mapped != nullptr)
			{
				l_Frame.Buffer->Unmap(0, nullptr);
				l_Frame.Mapped = nullptr;
			}

			l_Frame.Buffer.Reset();
			l_Frame.GpuAddress = 0;
		}

		if (m_Initialized)
		{
			PT_CORE_INFO("Upload ring shut down, high water {} KB of {} MB", m_HighWater / 1024, m_BytesPerFrame / (1024 * 1024));
		}

		m_BytesPerFrame = 0;
		m_Offset = 0;
		m_HighWater = 0;
		m_Initialized = false;
	}

	void D3D12UploadRing::BeginFrame(uint32_t frameIndex)
	{
		PT_CORE_ASSERT(frameIndex < D3D12::k_FramesInFlight, "Frame index {} out of range", frameIndex);

		m_FrameIndex = frameIndex;
		m_Offset = 0;
		m_OverflowWarned = false;
	}

	UploadAllocation D3D12UploadRing::Allocate(uint64_t size, uint64_t alignment)
	{
		PT_CORE_ASSERT(alignment != 0 && (alignment & (alignment - 1)) == 0, "Upload alignment {} is not a power of two", alignment);

		if (!m_Initialized || size == 0)
		{
			return UploadAllocation();
		}

		const uint64_t l_Start = (m_Offset + alignment - 1) & ~(alignment - 1);
		if (l_Start + size > m_BytesPerFrame)
		{
			if (!m_OverflowWarned)
			{
				PT_CORE_ERROR("Upload ring out of memory: {} bytes requested with {} of {} used; raise the per-frame size", size, m_Offset, m_BytesPerFrame);
				m_OverflowWarned = true;
			}

			return UploadAllocation();
		}

		m_Offset = l_Start + size;
		m_HighWater = std::max(m_HighWater, m_Offset);

		const FrameBuffer& l_Frame = m_Frames[m_FrameIndex];

		UploadAllocation l_Allocation;
		l_Allocation.Cpu = l_Frame.Mapped + l_Start;
		l_Allocation.Gpu = l_Frame.GpuAddress + l_Start;
		l_Allocation.Size = size;

		return l_Allocation;
	}
}
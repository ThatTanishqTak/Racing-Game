#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace Powertrain
{
	class D3D12Device;

	struct UploadAllocation
	{
		void* Cpu = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS Gpu = 0;
		ID3D12Resource* Resource = nullptr;
		uint64_t Offset = 0;
		uint64_t Size = 0;

		bool IsValid() const { return Cpu != nullptr; }
	};

	class D3D12UploadRing
	{
	public:
		static constexpr uint64_t k_ConstantAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

		D3D12UploadRing() = default;
		~D3D12UploadRing();

		D3D12UploadRing(const D3D12UploadRing&) = delete;
		D3D12UploadRing& operator=(const D3D12UploadRing&) = delete;

		bool Initialize(D3D12Device& device, uint64_t bytesPerFrame, std::string_view name);

		// Call after the direct queue has been flushed; the buffers are unmapped and released here
		void Shutdown();

		void BeginFrame(uint32_t frameIndex);

		// Returns an invalid allocation once the frame's budget is spent; the first overflow of a frame is logged
		UploadAllocation Allocate(uint64_t size, uint64_t alignment = k_ConstantAlignment);

		// Copies one constant struct into a fresh 256-byte aligned slice
		template<typename T>
		UploadAllocation Upload(const T& data)
		{
			const UploadAllocation l_Allocation = Allocate(sizeof(T));
			if (l_Allocation.IsValid())
			{
				std::memcpy(l_Allocation.Cpu, &data, sizeof(T));
			}

			return l_Allocation;
		}

		uint64_t GetBytesPerFrame() const { return m_BytesPerFrame; }
		uint64_t GetBytesInUse() const { return m_Offset; }
		uint64_t GetHighWater() const { return m_HighWater; }

	private:
		struct FrameBuffer
		{
			ComPtr<ID3D12Resource> Buffer;
			uint8_t* Mapped = nullptr;
			D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = 0;
		};

	private:
		std::array<FrameBuffer, D3D12::k_FramesInFlight> m_Frames;

		uint64_t m_BytesPerFrame = 0;
		uint64_t m_Offset = 0;
		uint64_t m_HighWater = 0;
		uint32_t m_FrameIndex = 0;

		bool m_Initialized = false;
		bool m_OverflowWarned = false;
	};
}
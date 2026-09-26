#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace Powertrain
{
	class D3D12Device;
	class D3D12CommandQueue;

	class D3D12GpuTimer
	{
	public:
		static constexpr uint32_t k_MaxTimers = 16;
		static constexpr uint32_t k_FrameTimer = 0;

		D3D12GpuTimer() = default;
		~D3D12GpuTimer();

		D3D12GpuTimer(const D3D12GpuTimer&) = delete;
		D3D12GpuTimer& operator=(const D3D12GpuTimer&) = delete;

		bool Initialize(D3D12Device& device, D3D12CommandQueue& queue, std::string_view name);
		void Shutdown();

		void BeginFrame(uint32_t frameIndex);
		void Begin(ID3D12GraphicsCommandList* commandList, uint32_t timer);
		void End(ID3D12GraphicsCommandList* commandList, uint32_t timer);
		void Resolve(ID3D12GraphicsCommandList* commandList);

		double GetMilliseconds(uint32_t timer) const;

		bool IsSupported() const { return m_Frequency != 0; }

	private:
		uint32_t QueryIndex(uint32_t frameIndex, uint32_t timer) const { return (frameIndex * k_MaxTimers + timer) * 2; }
		void ReadBack(uint32_t frameIndex);

	private:
		ComPtr<ID3D12QueryHeap> m_QueryHeap;
		ComPtr<ID3D12Resource> m_ReadbackBuffer;

		uint64_t m_Frequency = 0;
		uint32_t m_FrameIndex = 0;

		std::array<std::array<bool, k_MaxTimers>, D3D12::k_FramesInFlight> m_Used = {};
		std::array<std::array<bool, k_MaxTimers>, D3D12::k_FramesInFlight> m_Ended = {};
		std::array<double, k_MaxTimers> m_Results = {};
	};
}
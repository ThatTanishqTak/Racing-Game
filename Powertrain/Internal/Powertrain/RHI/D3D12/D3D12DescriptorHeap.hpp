#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Powertrain
{
	class D3D12Device;

	struct DescriptorHandle
	{
		D3D12_CPU_DESCRIPTOR_HANDLE CPU = {};
		D3D12_GPU_DESCRIPTOR_HANDLE GPU = {};
		uint32_t Index = UINT32_MAX;

		bool IsValid() const { return Index != UINT32_MAX; }
		bool IsShaderVisible() const { return GPU.ptr != 0; }
	};

	class D3D12DescriptorHeap
	{
	public:
		D3D12DescriptorHeap() = default;
		~D3D12DescriptorHeap();

		D3D12DescriptorHeap(const D3D12DescriptorHeap&) = delete;
		D3D12DescriptorHeap& operator=(const D3D12DescriptorHeap&) = delete;

		bool Initialize(D3D12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t persistentCount, uint32_t transientCountPerFrame, bool shaderVisible, std::string_view name);
		void Shutdown();

		// Persistent descriptors live until Free
		DescriptorHandle Allocate();
		void Free(DescriptorHandle& handle);

		// Transient descriptors live until the same frame index comes around again
		void BeginFrame(uint32_t frameIndex);
		DescriptorHandle AllocateTransient(uint32_t count = 1);

		ID3D12DescriptorHeap* GetHandle() const { return m_Heap.Get(); }
		D3D12_DESCRIPTOR_HEAP_TYPE GetType() const { return m_Type; }
		uint32_t GetDescriptorSize() const { return m_DescriptorSize; }
		bool IsShaderVisible() const { return m_GpuStart.ptr != 0; }

		uint32_t GetPersistentCount() const { return m_PersistentCount; }
		uint32_t GetPersistentInUse() const { return m_PersistentInUse; }
		uint32_t GetTransientCountPerFrame() const { return m_TransientCountPerFrame; }
		uint32_t GetTransientInUse() const { return m_TransientInUse; }

	private:
		DescriptorHandle MakeHandle(uint32_t index) const;

	private:
		ComPtr<ID3D12DescriptorHeap> m_Heap;
		D3D12_DESCRIPTOR_HEAP_TYPE m_Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		D3D12_CPU_DESCRIPTOR_HANDLE m_CpuStart = {};
		D3D12_GPU_DESCRIPTOR_HANDLE m_GpuStart = {};
		uint32_t m_DescriptorSize = 0;

		uint32_t m_PersistentCount = 0;
		uint32_t m_PersistentHighWater = 0;
		uint32_t m_PersistentInUse = 0;
		std::vector<uint32_t> m_FreeList;

		uint32_t m_TransientCountPerFrame = 0;
		uint32_t m_TransientInUse = 0;
		uint32_t m_FrameIndex = 0;
	};
}
#pragma once

#include "Powertrain/Core/Handle.hpp"
#include "Powertrain/Math/BoundingBox.hpp"
#include "Powertrain/Renderer/RenderTypes.hpp"
#include "Powertrain/RHI/D3D12/D3D12CommandList.hpp"
#include "Powertrain/RHI/D3D12/D3D12Common.hpp"
#include "Powertrain/RHI/D3D12/D3D12DescriptorHeap.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Powertrain
{
	class D3D12CommandQueue;
	class D3D12DeferredReleaseQueue;
	class D3D12Device;

	struct D3D12Mesh
	{
		ComPtr<ID3D12Resource> VertexBuffer;
		ComPtr<ID3D12Resource> IndexBuffer;
		DescriptorHandle VertexView;
		D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};

		uint32_t VertexCount = 0;
		uint32_t IndexCount = 0;
		std::vector<Submesh> Submeshes;
		BoundingBox Bounds = BoundingBox::Empty();
	};

	class D3D12MeshStorage
	{
	public:
		D3D12MeshStorage() = default;
		~D3D12MeshStorage();

		D3D12MeshStorage(const D3D12MeshStorage&) = delete;
		D3D12MeshStorage& operator=(const D3D12MeshStorage&) = delete;

		bool Initialize(D3D12Device& device, D3D12CommandQueue& copyQueue, D3D12DescriptorHeap& resourceHeap, D3D12DeferredReleaseQueue& deferredRelease);
		void Shutdown();

		MeshHandle Create(const MeshData& data, std::string_view name = "Mesh");

		void Destroy(MeshHandle handle, uint64_t fenceValue);
		void ReleaseCompleted(uint64_t completedFenceValue);

		const D3D12Mesh* Get(MeshHandle handle) const;

		uint32_t GetAliveCount() const { return m_AliveCount; }
		uint64_t GetGpuBytes() const { return m_GpuBytes; }

	private:
		struct Slot
		{
			D3D12Mesh Mesh;
			uint32_t Generation = 1;
			uint64_t GpuBytes = 0;
			bool Alive = false;
		};

		struct PendingDescriptor
		{
			DescriptorHandle Handle;
			uint64_t FenceValue = 0;
		};

		bool CreateBuffer(uint64_t bytes, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState, std::string_view name, ComPtr<ID3D12Resource>& buffer);
		bool UploadBuffers(const MeshData& data, D3D12Mesh& mesh, std::string_view name);

	private:
		D3D12Device* m_Device = nullptr;
		D3D12CommandQueue* m_CopyQueue = nullptr;
		D3D12DescriptorHeap* m_ResourceHeap = nullptr;
		D3D12DeferredReleaseQueue* m_DeferredRelease = nullptr;

		D3D12CommandList m_CopyList;

		std::vector<Slot> m_Slots;
		std::vector<uint32_t> m_FreeSlots;
		std::vector<PendingDescriptor> m_PendingDescriptors;

		uint32_t m_AliveCount = 0;
		uint64_t m_GpuBytes = 0;

		bool m_Initialized = false;
	};
}
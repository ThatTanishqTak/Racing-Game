#include "Powertrain/RHI/D3D12/D3D12MeshStorage.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/RHI/D3D12/D3D12CommandQueue.hpp"
#include "Powertrain/RHI/D3D12/D3D12DeferredReleaseQueue.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

#include <cstring>
#include <format>

namespace Powertrain
{
	D3D12MeshStorage::~D3D12MeshStorage()
	{
		Shutdown();
	}

	bool D3D12MeshStorage::Initialize(D3D12Device& device, D3D12CommandQueue& copyQueue, D3D12DescriptorHeap& resourceHeap, D3D12DeferredReleaseQueue& deferredRelease)
	{
		PT_CORE_ASSERT(copyQueue.GetType() == D3D12_COMMAND_LIST_TYPE_COPY, "Mesh storage uploads through the copy queue");

		m_Device = &device;
		m_CopyQueue = &copyQueue;
		m_ResourceHeap = &resourceHeap;
		m_DeferredRelease = &deferredRelease;

		if (!m_CopyList.Initialize(device, D3D12_COMMAND_LIST_TYPE_COPY, "Mesh Upload List"))
		{
			Shutdown();

			return false;
		}

		m_Slots.reserve(64);
		m_AliveCount = 0;
		m_GpuBytes = 0;
		m_Initialized = true;

		PT_CORE_INFO("Mesh storage ready");

		return true;
	}

	void D3D12MeshStorage::Shutdown()
	{
		if (m_ResourceHeap != nullptr)
		{
			for (Slot& l_Slot : m_Slots)
			{
				m_ResourceHeap->Free(l_Slot.Mesh.VertexView);
				l_Slot.Mesh.VertexBuffer.Reset();
				l_Slot.Mesh.IndexBuffer.Reset();
				l_Slot.Alive = false;
			}

			for (PendingDescriptor& l_Pending : m_PendingDescriptors)
			{
				m_ResourceHeap->Free(l_Pending.Handle);
			}
		}

		if (m_Initialized && m_AliveCount != 0)
		{
			PT_CORE_WARN("Mesh storage shut down with {} meshes still alive", m_AliveCount);
		}

		m_CopyList.Shutdown();
		m_Slots.clear();
		m_FreeSlots.clear();
		m_PendingDescriptors.clear();
		m_AliveCount = 0;
		m_GpuBytes = 0;

		m_Device = nullptr;
		m_CopyQueue = nullptr;
		m_ResourceHeap = nullptr;
		m_DeferredRelease = nullptr;

		if (m_Initialized)
		{
			PT_CORE_INFO("Mesh storage shut down");
		}

		m_Initialized = false;
	}

	MeshHandle D3D12MeshStorage::Create(const MeshData& data, std::string_view name)
	{
		if (!m_Initialized)
		{
			return MeshHandle();
		}

		if (data.Vertices.empty() || data.Indices.empty() || data.Indices.size() % 3 != 0)
		{
			PT_CORE_ERROR("Mesh '{}' rejected: {} vertices and {} indices", name, data.Vertices.size(), data.Indices.size());

			return MeshHandle();
		}

		for (const uint32_t l_Index : data.Indices)
		{
			if (l_Index >= data.Vertices.size())
			{
				PT_CORE_ERROR("Mesh '{}' rejected: index {} exceeds {} vertices", name, l_Index, data.Vertices.size());

				return MeshHandle();
			}
		}

		D3D12Mesh l_Mesh;
		l_Mesh.VertexCount = static_cast<uint32_t>(data.Vertices.size());
		l_Mesh.IndexCount = static_cast<uint32_t>(data.Indices.size());
		l_Mesh.Submeshes = data.Submeshes;
		if (l_Mesh.Submeshes.empty())
		{
			l_Mesh.Submeshes.push_back({ 0, l_Mesh.IndexCount, 0 });
		}

		for (const Submesh& l_Submesh : l_Mesh.Submeshes)
		{
			if (static_cast<uint64_t>(l_Submesh.IndexOffset) + l_Submesh.IndexCount > l_Mesh.IndexCount)
			{
				PT_CORE_ERROR("Mesh '{}' rejected: submesh covers indices {} to {} of {}", name, l_Submesh.IndexOffset, l_Submesh.IndexOffset + l_Submesh.IndexCount, l_Mesh.IndexCount);

				return MeshHandle();
			}
		}

		for (const Vertex& l_Vertex : data.Vertices)
		{
			l_Mesh.Bounds.Encapsulate(l_Vertex.Position);
		}

		if (!UploadBuffers(data, l_Mesh, name))
		{
			m_ResourceHeap->Free(l_Mesh.VertexView);

			return MeshHandle();
		}

		uint32_t l_SlotIndex;
		if (!m_FreeSlots.empty())
		{
			l_SlotIndex = m_FreeSlots.back();
			m_FreeSlots.pop_back();
		}
		else
		{
			l_SlotIndex = static_cast<uint32_t>(m_Slots.size());
			m_Slots.emplace_back();
		}

		Slot& l_Slot = m_Slots[l_SlotIndex];
		l_Slot.Mesh = std::move(l_Mesh);
		l_Slot.GpuBytes = static_cast<uint64_t>(l_Slot.Mesh.VertexCount) * sizeof(Vertex) + static_cast<uint64_t>(l_Slot.Mesh.IndexCount) * sizeof(uint32_t);
		l_Slot.Alive = true;

		++m_AliveCount;
		m_GpuBytes += l_Slot.GpuBytes;

		PT_CORE_TRACE("Mesh '{}' created: {} vertices, {} triangles, {} KB, descriptor {}", name, l_Slot.Mesh.VertexCount, l_Slot.Mesh.IndexCount / 3, l_Slot.GpuBytes / 1024, l_Slot.Mesh.VertexView.Index);

		return MeshHandle{ l_SlotIndex, l_Slot.Generation };
	}

	void D3D12MeshStorage::Destroy(MeshHandle handle, uint64_t fenceValue)
	{
		if (!m_Initialized || Get(handle) == nullptr)
		{
			return;
		}

		Slot& l_Slot = m_Slots[handle.Index];

		m_DeferredRelease->Enqueue(l_Slot.Mesh.VertexBuffer, fenceValue);
		m_DeferredRelease->Enqueue(l_Slot.Mesh.IndexBuffer, fenceValue);
		m_PendingDescriptors.push_back({ l_Slot.Mesh.VertexView, fenceValue });

		m_GpuBytes -= l_Slot.GpuBytes;
		--m_AliveCount;

		l_Slot.Mesh = D3D12Mesh();
		l_Slot.GpuBytes = 0;
		l_Slot.Alive = false;
		++l_Slot.Generation;

		m_FreeSlots.push_back(handle.Index);
	}

	void D3D12MeshStorage::ReleaseCompleted(uint64_t completedFenceValue)
	{
		for (size_t l_Index = 0; l_Index < m_PendingDescriptors.size();)
		{
			if (m_PendingDescriptors[l_Index].FenceValue <= completedFenceValue)
			{
				m_ResourceHeap->Free(m_PendingDescriptors[l_Index].Handle);
				m_PendingDescriptors[l_Index] = m_PendingDescriptors.back();
				m_PendingDescriptors.pop_back();
			}
			else
			{
				++l_Index;
			}
		}
	}

	const D3D12Mesh* D3D12MeshStorage::Get(MeshHandle handle) const
	{
		if (!handle.IsValid() || handle.Index >= m_Slots.size())
		{
			return nullptr;
		}

		const Slot& l_Slot = m_Slots[handle.Index];
		if (!l_Slot.Alive || l_Slot.Generation != handle.Generation)
		{
			return nullptr;
		}

		return &l_Slot.Mesh;
	}

	bool D3D12MeshStorage::CreateBuffer(uint64_t bytes, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState, std::string_view name, ComPtr<ID3D12Resource>& buffer)
	{
		D3D12_HEAP_PROPERTIES l_HeapProperties = {};
		l_HeapProperties.Type = heapType;

		D3D12_RESOURCE_DESC l_Description = {};
		l_Description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		l_Description.Width = bytes;
		l_Description.Height = 1;
		l_Description.DepthOrArraySize = 1;
		l_Description.MipLevels = 1;
		l_Description.Format = DXGI_FORMAT_UNKNOWN;
		l_Description.SampleDesc = { 1, 0 };
		l_Description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		l_Description.Flags = D3D12_RESOURCE_FLAG_NONE;

		if (!D3D12::CheckResult(m_Device->GetHandle()->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_Description, initialState, nullptr, IID_PPV_ARGS(&buffer)), std::format("CreateCommittedResource ({})", name)))
		{
			return false;
		}

		D3D12::SetDebugName(buffer.Get(), name);

		return true;
	}

	bool D3D12MeshStorage::UploadBuffers(const MeshData& data, D3D12Mesh& mesh, std::string_view name)
	{
		const uint64_t l_VertexBytes = static_cast<uint64_t>(mesh.VertexCount) * sizeof(Vertex);
		const uint64_t l_IndexBytes = static_cast<uint64_t>(mesh.IndexCount) * sizeof(uint32_t);

		if (!CreateBuffer(l_VertexBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, std::format("{} Vertices", name), mesh.VertexBuffer) || !CreateBuffer(l_IndexBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, std::format("{} Indices", name), mesh.IndexBuffer))
		{
			return false;
		}

		ComPtr<ID3D12Resource> l_Staging;
		if (!CreateBuffer(l_VertexBytes + l_IndexBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, std::format("{} Staging", name), l_Staging))
		{
			return false;
		}

		const D3D12_RANGE l_NoRead = { 0, 0 };
		void* l_Mapped = nullptr;
		if (!D3D12::CheckResult(l_Staging->Map(0, &l_NoRead, &l_Mapped), std::format("ID3D12Resource::Map ({} staging)", name)))
		{
			return false;
		}

		std::memcpy(l_Mapped, data.Vertices.data(), l_VertexBytes);
		std::memcpy(static_cast<uint8_t*>(l_Mapped) + l_VertexBytes, data.Indices.data(), l_IndexBytes);
		l_Staging->Unmap(0, nullptr);

		if (!m_CopyList.Reset(0))
		{
			return false;
		}

		ID3D12GraphicsCommandList* l_List = m_CopyList.GetHandle();
		l_List->CopyBufferRegion(mesh.VertexBuffer.Get(), 0, l_Staging.Get(), 0, l_VertexBytes);
		l_List->CopyBufferRegion(mesh.IndexBuffer.Get(), 0, l_Staging.Get(), l_VertexBytes, l_IndexBytes);

		if (!m_CopyList.Close())
		{
			return false;
		}

		m_CopyQueue->WaitForFence(m_CopyQueue->ExecuteCommandList(l_List));

		D3D12_SHADER_RESOURCE_VIEW_DESC l_View = {};
		l_View.Format = DXGI_FORMAT_R32_TYPELESS;
		l_View.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		l_View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		l_View.Buffer.FirstElement = 0;
		l_View.Buffer.NumElements = static_cast<UINT>(l_VertexBytes / sizeof(uint32_t));
		l_View.Buffer.StructureByteStride = 0;
		l_View.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

		mesh.VertexView = m_ResourceHeap->Allocate();
		if (!mesh.VertexView.IsValid())
		{
			return false;
		}

		m_Device->GetHandle()->CreateShaderResourceView(mesh.VertexBuffer.Get(), &l_View, mesh.VertexView.CPU);

		mesh.IndexBufferView.BufferLocation = mesh.IndexBuffer->GetGPUVirtualAddress();
		mesh.IndexBufferView.SizeInBytes = static_cast<UINT>(l_IndexBytes);
		mesh.IndexBufferView.Format = DXGI_FORMAT_R32_UINT;

		return true;
	}
}
#pragma once

#include "Powertrain/Core/Handle.hpp"
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

	struct D3D12Texture
	{
		ComPtr<ID3D12Resource> Resource;
		DescriptorHandle View;

		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t MipCount = 0;
		DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
	};

	class D3D12TextureStorage
	{
	public:
		D3D12TextureStorage() = default;
		~D3D12TextureStorage();

		D3D12TextureStorage(const D3D12TextureStorage&) = delete;
		D3D12TextureStorage& operator=(const D3D12TextureStorage&) = delete;

		bool Initialize(D3D12Device& device, D3D12CommandQueue& copyQueue, D3D12DescriptorHeap& resourceHeap, D3D12DeferredReleaseQueue& deferredRelease);
		void Shutdown();

		TextureHandle Create(const TextureData& data, std::string_view name = "Texture");

		void Destroy(TextureHandle handle, uint64_t fenceValue);
		void ReleaseCompleted(uint64_t completedFenceValue);

		const D3D12Texture* Get(TextureHandle handle) const;

		uint32_t GetDescriptorIndex(TextureHandle handle, TextureHandle fallback) const;

		TextureHandle GetWhiteTexture() const { return m_White; }
		TextureHandle GetFlatNormalTexture() const { return m_FlatNormal; }

		uint32_t GetAliveCount() const { return m_AliveCount; }
		uint64_t GetGpuBytes() const { return m_GpuBytes; }

	private:
		struct Slot
		{
			D3D12Texture Texture;
			uint32_t Generation = 1;
			uint64_t GpuBytes = 0;
			bool Alive = false;
		};

		struct PendingDescriptor
		{
			DescriptorHandle Handle;
			uint64_t FenceValue = 0;
		};

		bool CreateDefaults();
		bool Upload(const TextureData& data, D3D12Texture& texture, uint64_t& gpuBytes, std::string_view name);

	private:
		D3D12Device* m_Device = nullptr;
		D3D12CommandQueue* m_CopyQueue = nullptr;
		D3D12DescriptorHeap* m_ResourceHeap = nullptr;
		D3D12DeferredReleaseQueue* m_DeferredRelease = nullptr;

		D3D12CommandList m_CopyList;

		std::vector<Slot> m_Slots;
		std::vector<uint32_t> m_FreeSlots;
		std::vector<PendingDescriptor> m_PendingDescriptors;

		TextureHandle m_White;
		TextureHandle m_FlatNormal;

		uint32_t m_AliveCount = 0;
		uint64_t m_GpuBytes = 0;

		bool m_Initialized = false;
	};
}
#include "Powertrain/RHI/D3D12/D3D12TextureStorage.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12CommandQueue.hpp"
#include "Powertrain/RHI/D3D12/D3D12DeferredReleaseQueue.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <format>

namespace Powertrain
{
	namespace
	{
		struct FormatInfo
		{
			DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
			uint32_t BlockSize = 1;
			uint32_t BytesPerBlock = 4;
			bool Srgb = false;
			bool CanGenerateMips = false;
		};

		FormatInfo GetFormatInfo(TextureFormat format)
		{
			switch (format)
			{
				case TextureFormat::RGBA8Unorm:
				{
					return { DXGI_FORMAT_R8G8B8A8_UNORM, 1, 4, false, true };
				}
				case TextureFormat::RGBA8Srgb:
				{
					return { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1, 4, true, true };
				}
				case TextureFormat::RGBA16Float:
				{
					return { DXGI_FORMAT_R16G16B16A16_FLOAT, 1, 8, false, true };
				}
				case TextureFormat::BC7Unorm:
				{
					return { DXGI_FORMAT_BC7_UNORM, 4, 16, false, false };
				}
				case TextureFormat::BC7Srgb:
				{
					return { DXGI_FORMAT_BC7_UNORM_SRGB, 4, 16, true, false };
				}
			}

			return {};
		}

		uint32_t MipDimension(uint32_t size, uint32_t mip)
		{
			return std::max(1u, size >> mip);
		}

		uint64_t MipBytes(const FormatInfo& info, uint32_t width, uint32_t height)
		{
			const uint64_t l_BlocksWide = (width + info.BlockSize - 1) / info.BlockSize;
			const uint64_t l_BlocksHigh = (height + info.BlockSize - 1) / info.BlockSize;

			return l_BlocksWide * l_BlocksHigh * info.BytesPerBlock;
		}

		uint32_t FullMipCount(uint32_t width, uint32_t height)
		{
			return static_cast<uint32_t>(std::bit_width(std::max(width, height)));
		}

		float DecodeSrgb(uint8_t value)
		{
			const float l_Encoded = static_cast<float>(value) / 255.0f;

			return l_Encoded <= 0.04045f ? l_Encoded / 12.92f : std::pow((l_Encoded + 0.055f) / 1.055f, 2.4f);
		}

		uint8_t EncodeSrgb(float linear)
		{
			const float l_Clamped = std::clamp(linear, 0.0f, 1.0f);
			const float l_Encoded = l_Clamped <= 0.0031308f ? l_Clamped * 12.92f : 1.055f * std::pow(l_Clamped, 1.0f / 2.4f) - 0.055f;

			return static_cast<uint8_t>(l_Encoded * 255.0f + 0.5f);
		}

		float HalfToFloat(uint16_t half)
		{
			const uint32_t l_Sign = (half & 0x8000u) << 16;
			uint32_t l_Exponent = (half >> 10) & 0x1Fu;
			uint32_t l_Mantissa = half & 0x3FFu;

			if (l_Exponent == 0)
			{
				if (l_Mantissa == 0)
				{
					return std::bit_cast<float>(l_Sign);
				}

				while ((l_Mantissa & 0x400u) == 0)
				{
					l_Mantissa <<= 1;
					--l_Exponent;
				}

				++l_Exponent;
				l_Mantissa &= 0x3FFu;
			}
			else if (l_Exponent == 0x1Fu)
			{
				return std::bit_cast<float>(l_Sign | 0x7F800000u | (l_Mantissa << 13));
			}

			return std::bit_cast<float>(l_Sign | ((l_Exponent + 112u) << 23) | (l_Mantissa << 13));
		}

		uint16_t FloatToHalf(float value)
		{
			const uint32_t l_Bits = std::bit_cast<uint32_t>(value);
			const uint16_t l_Sign = static_cast<uint16_t>((l_Bits >> 16) & 0x8000u);
			const int32_t l_Exponent = static_cast<int32_t>((l_Bits >> 23) & 0xFFu) - 127 + 15;
			const uint32_t l_Mantissa = l_Bits & 0x7FFFFFu;

			if (l_Exponent >= 0x1F)
			{
				return static_cast<uint16_t>(l_Sign | 0x7C00u | (l_Mantissa != 0 && ((l_Bits >> 23) & 0xFFu) == 0xFFu ? 0x200u : 0u));
			}

			if (l_Exponent <= 0)
			{
				if (l_Exponent < -10)
				{
					return l_Sign;
				}

				const uint32_t l_Shifted = (l_Mantissa | 0x800000u) >> (1 - l_Exponent);

				return static_cast<uint16_t>(l_Sign | ((l_Shifted + 0x1000u) >> 13));
			}

			return static_cast<uint16_t>(l_Sign | (static_cast<uint32_t>(l_Exponent) << 10) | ((l_Mantissa + 0x1000u) >> 13));
		}

		void ReadTexel(const uint8_t* source, const FormatInfo& info, float* out)
		{
			if (info.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
			{
				const uint16_t* l_Halves = reinterpret_cast<const uint16_t*>(source);
				for (int l_Channel = 0; l_Channel < 4; ++l_Channel)
				{
					out[l_Channel] = HalfToFloat(l_Halves[l_Channel]);
				}

				return;
			}

			for (int l_Channel = 0; l_Channel < 3; ++l_Channel)
			{
				out[l_Channel] = info.Srgb ? DecodeSrgb(source[l_Channel]) : static_cast<float>(source[l_Channel]) / 255.0f;
			}

			out[3] = static_cast<float>(source[3]) / 255.0f;
		}

		void WriteTexel(uint8_t* destination, const FormatInfo& info, const float* in)
		{
			if (info.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
			{
				uint16_t* l_Halves = reinterpret_cast<uint16_t*>(destination);
				for (int l_Channel = 0; l_Channel < 4; ++l_Channel)
				{
					l_Halves[l_Channel] = FloatToHalf(in[l_Channel]);
				}

				return;
			}

			for (int l_Channel = 0; l_Channel < 3; ++l_Channel)
			{
				destination[l_Channel] = info.Srgb ? EncodeSrgb(in[l_Channel]) : static_cast<uint8_t>(std::clamp(in[l_Channel], 0.0f, 1.0f) * 255.0f + 0.5f);
			}

			destination[3] = static_cast<uint8_t>(std::clamp(in[3], 0.0f, 1.0f) * 255.0f + 0.5f);
		}

		// Box filter in linear space, level by level, appended tightly after the level it came from
		void GenerateMips(TextureData& data, const FormatInfo& info)
		{
			const uint32_t l_MipCount = FullMipCount(data.Width, data.Height);
			if (l_MipCount <= 1)
			{
				return;
			}

			uint64_t l_TotalBytes = 0;
			for (uint32_t l_Mip = 0; l_Mip < l_MipCount; ++l_Mip)
			{
				l_TotalBytes += MipBytes(info, MipDimension(data.Width, l_Mip), MipDimension(data.Height, l_Mip));
			}

			data.Pixels.resize(l_TotalBytes);

			uint64_t l_SourceOffset = 0;
			for (uint32_t l_Mip = 1; l_Mip < l_MipCount; ++l_Mip)
			{
				const uint32_t l_SourceWidth = MipDimension(data.Width, l_Mip - 1);
				const uint32_t l_SourceHeight = MipDimension(data.Height, l_Mip - 1);
				const uint32_t l_Width = MipDimension(data.Width, l_Mip);
				const uint32_t l_Height = MipDimension(data.Height, l_Mip);
				const uint64_t l_DestinationOffset = l_SourceOffset + MipBytes(info, l_SourceWidth, l_SourceHeight);

				for (uint32_t l_Y = 0; l_Y < l_Height; ++l_Y)
				{
					for (uint32_t l_X = 0; l_X < l_Width; ++l_X)
					{
						float l_Sum[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						for (uint32_t l_Dy = 0; l_Dy < 2; ++l_Dy)
						{
							for (uint32_t l_Dx = 0; l_Dx < 2; ++l_Dx)
							{
								const uint32_t l_SourceX = std::min(l_X * 2 + l_Dx, l_SourceWidth - 1);
								const uint32_t l_SourceY = std::min(l_Y * 2 + l_Dy, l_SourceHeight - 1);

								float l_Texel[4];
								ReadTexel(&data.Pixels[l_SourceOffset + (static_cast<uint64_t>(l_SourceY) * l_SourceWidth + l_SourceX) * info.BytesPerBlock], info, l_Texel);
								for (int l_Channel = 0; l_Channel < 4; ++l_Channel)
								{
									l_Sum[l_Channel] += l_Texel[l_Channel] * 0.25f;
								}
							}
						}

						WriteTexel(&data.Pixels[l_DestinationOffset + (static_cast<uint64_t>(l_Y) * l_Width + l_X) * info.BytesPerBlock], info, l_Sum);
					}
				}

				l_SourceOffset = l_DestinationOffset;
			}

			data.MipCount = l_MipCount;
		}

		bool CreateBuffer(ID3D12Device* device, uint64_t bytes, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState, std::string_view name, ComPtr<ID3D12Resource>& buffer)
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

			if (!D3D12::CheckResult(device->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_Description, initialState, nullptr, IID_PPV_ARGS(&buffer)), std::format("CreateCommittedResource ({})", name)))
			{
				return false;
			}

			D3D12::SetDebugName(buffer.Get(), name);

			return true;
		}
	}

	D3D12TextureStorage::~D3D12TextureStorage()
	{
		Shutdown();
	}

	bool D3D12TextureStorage::Initialize(D3D12Device& device, D3D12CommandQueue& copyQueue, D3D12DescriptorHeap& resourceHeap, D3D12DeferredReleaseQueue& deferredRelease)
	{
		PT_CORE_ASSERT(copyQueue.GetType() == D3D12_COMMAND_LIST_TYPE_COPY, "Texture storage uploads through the copy queue");

		m_Device = &device;
		m_CopyQueue = &copyQueue;
		m_ResourceHeap = &resourceHeap;
		m_DeferredRelease = &deferredRelease;

		if (!m_CopyList.Initialize(device, D3D12_COMMAND_LIST_TYPE_COPY, "Texture Upload List"))
		{
			Shutdown();

			return false;
		}

		m_Slots.reserve(256);
		m_AliveCount = 0;
		m_GpuBytes = 0;
		m_Initialized = true;

		if (!CreateDefaults())
		{
			Shutdown();

			return false;
		}

		PT_CORE_INFO("Texture storage ready");

		return true;
	}

	void D3D12TextureStorage::Shutdown()
	{
		if (m_ResourceHeap != nullptr)
		{
			for (Slot& l_Slot : m_Slots)
			{
				m_ResourceHeap->Free(l_Slot.Texture.View);
				l_Slot.Texture.Resource.Reset();
				l_Slot.Alive = false;
			}

			for (PendingDescriptor& l_Pending : m_PendingDescriptors)
			{
				m_ResourceHeap->Free(l_Pending.Handle);
			}
		}

		if (m_Initialized && m_AliveCount > 2)
		{
			PT_CORE_WARN("Texture storage shut down with {} textures still alive", m_AliveCount - 2);
		}

		m_CopyList.Shutdown();
		m_Slots.clear();
		m_FreeSlots.clear();
		m_PendingDescriptors.clear();
		m_White = TextureHandle();
		m_FlatNormal = TextureHandle();
		m_AliveCount = 0;
		m_GpuBytes = 0;

		m_Device = nullptr;
		m_CopyQueue = nullptr;
		m_ResourceHeap = nullptr;
		m_DeferredRelease = nullptr;

		if (m_Initialized)
		{
			PT_CORE_INFO("Texture storage shut down");
		}

		m_Initialized = false;
	}

	TextureHandle D3D12TextureStorage::Create(const TextureData& data, std::string_view name)
	{
		if (!m_Initialized)
		{
			return TextureHandle();
		}

		const FormatInfo l_Info = GetFormatInfo(data.Format);
		if (l_Info.Format == DXGI_FORMAT_UNKNOWN || data.Width == 0 || data.Height == 0 || data.MipCount == 0)
		{
			PT_CORE_ERROR("Texture '{}' rejected: {}x{} with {} mips", name, data.Width, data.Height, data.MipCount);

			return TextureHandle();
		}

		if (data.MipCount > FullMipCount(data.Width, data.Height))
		{
			PT_CORE_ERROR("Texture '{}' rejected: {} mips exceed the {} a {}x{} texture can hold", name, data.MipCount, FullMipCount(data.Width, data.Height), data.Width, data.Height);

			return TextureHandle();
		}

		uint64_t l_ExpectedBytes = 0;
		for (uint32_t l_Mip = 0; l_Mip < data.MipCount; ++l_Mip)
		{
			l_ExpectedBytes += MipBytes(l_Info, MipDimension(data.Width, l_Mip), MipDimension(data.Height, l_Mip));
		}

		if (data.Pixels.size() != l_ExpectedBytes)
		{
			PT_CORE_ERROR("Texture '{}' rejected: {} bytes of pixels, {} expected for {}x{} with {} mips", name, data.Pixels.size(), l_ExpectedBytes, data.Width, data.Height, data.MipCount);

			return TextureHandle();
		}

		const TextureData* l_Source = &data;
		TextureData l_WithMips;
		if (data.MipCount == 1 && l_Info.CanGenerateMips && (data.Width > 1 || data.Height > 1))
		{
			l_WithMips = data;
			GenerateMips(l_WithMips, l_Info);
			l_Source = &l_WithMips;
		}

		D3D12Texture l_Texture;
		uint64_t l_GpuBytes = 0;
		if (!Upload(*l_Source, l_Texture, l_GpuBytes, name))
		{
			m_ResourceHeap->Free(l_Texture.View);

			return TextureHandle();
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
		l_Slot.Texture = std::move(l_Texture);
		l_Slot.GpuBytes = l_GpuBytes;
		l_Slot.Alive = true;

		++m_AliveCount;
		m_GpuBytes += l_GpuBytes;

		PT_CORE_TRACE("Texture '{}' created: {}x{}, {} mips, {} KB, descriptor {}", name, l_Slot.Texture.Width, l_Slot.Texture.Height, l_Slot.Texture.MipCount, l_GpuBytes / 1024, l_Slot.Texture.View.Index);

		return TextureHandle{ l_SlotIndex, l_Slot.Generation };
	}

	void D3D12TextureStorage::Destroy(TextureHandle handle, uint64_t fenceValue)
	{
		if (!m_Initialized || Get(handle) == nullptr)
		{
			return;
		}

		if (handle == m_White || handle == m_FlatNormal)
		{
			PT_CORE_WARN("Ignoring DestroyTexture on a default texture");

			return;
		}

		Slot& l_Slot = m_Slots[handle.Index];

		m_DeferredRelease->Enqueue(l_Slot.Texture.Resource, fenceValue);
		m_PendingDescriptors.push_back({ l_Slot.Texture.View, fenceValue });

		m_GpuBytes -= l_Slot.GpuBytes;
		--m_AliveCount;

		l_Slot.Texture = D3D12Texture();
		l_Slot.GpuBytes = 0;
		l_Slot.Alive = false;
		++l_Slot.Generation;

		m_FreeSlots.push_back(handle.Index);
	}

	void D3D12TextureStorage::ReleaseCompleted(uint64_t completedFenceValue)
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

	const D3D12Texture* D3D12TextureStorage::Get(TextureHandle handle) const
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

		return &l_Slot.Texture;
	}

	uint32_t D3D12TextureStorage::GetDescriptorIndex(TextureHandle handle, TextureHandle fallback) const
	{
		const D3D12Texture* l_Texture = Get(handle);
		if (l_Texture == nullptr)
		{
			l_Texture = Get(fallback);
		}

		return l_Texture != nullptr ? l_Texture->View.Index : UINT32_MAX;
	}

	bool D3D12TextureStorage::CreateDefaults()
	{
		TextureData l_White;
		l_White.Width = 1;
		l_White.Height = 1;
		l_White.MipCount = 1;
		l_White.Format = TextureFormat::RGBA8Unorm;
		l_White.Pixels = { 255, 255, 255, 255 };

		TextureData l_FlatNormal;
		l_FlatNormal.Width = 1;
		l_FlatNormal.Height = 1;
		l_FlatNormal.MipCount = 1;
		l_FlatNormal.Format = TextureFormat::RGBA8Unorm;
		l_FlatNormal.Pixels = { 128, 128, 255, 255 };

		m_White = Create(l_White, "Default White");
		m_FlatNormal = Create(l_FlatNormal, "Default Flat Normal");

		return m_White.IsValid() && m_FlatNormal.IsValid();
	}

	bool D3D12TextureStorage::Upload(const TextureData& data, D3D12Texture& texture, uint64_t& gpuBytes, std::string_view name)
	{
		ID3D12Device* l_Device = m_Device->GetHandle();
		const FormatInfo l_Info = GetFormatInfo(data.Format);

		D3D12_HEAP_PROPERTIES l_HeapProperties = {};
		l_HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC l_Description = {};
		l_Description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		l_Description.Width = data.Width;
		l_Description.Height = data.Height;
		l_Description.DepthOrArraySize = 1;
		l_Description.MipLevels = static_cast<UINT16>(data.MipCount);
		l_Description.Format = l_Info.Format;
		l_Description.SampleDesc = { 1, 0 };
		l_Description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		l_Description.Flags = D3D12_RESOURCE_FLAG_NONE;

		if (!D3D12::CheckResult(l_Device->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_Description, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&texture.Resource)), std::format("CreateCommittedResource ({})", name)))
		{
			return false;
		}

		D3D12::SetDebugName(texture.Resource.Get(), name);

		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> l_Footprints(data.MipCount);
		std::vector<UINT> l_RowCounts(data.MipCount);
		std::vector<UINT64> l_RowBytes(data.MipCount);
		UINT64 l_StagingBytes = 0;
		l_Device->GetCopyableFootprints(&l_Description, 0, data.MipCount, 0, l_Footprints.data(), l_RowCounts.data(), l_RowBytes.data(), &l_StagingBytes);

		ComPtr<ID3D12Resource> l_Staging;
		if (!CreateBuffer(l_Device, l_StagingBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, std::format("{} Staging", name), l_Staging))
		{
			return false;
		}

		const D3D12_RANGE l_NoRead = { 0, 0 };
		void* l_Mapped = nullptr;
		if (!D3D12::CheckResult(l_Staging->Map(0, &l_NoRead, &l_Mapped), std::format("ID3D12Resource::Map ({} staging)", name)))
		{
			return false;
		}

		uint64_t l_SourceOffset = 0;
		for (uint32_t l_Mip = 0; l_Mip < data.MipCount; ++l_Mip)
		{
			const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& l_Footprint = l_Footprints[l_Mip];
			uint8_t* l_Destination = static_cast<uint8_t*>(l_Mapped) + l_Footprint.Offset;

			for (UINT l_Row = 0; l_Row < l_RowCounts[l_Mip]; ++l_Row)
			{
				std::memcpy(l_Destination + static_cast<uint64_t>(l_Row) * l_Footprint.Footprint.RowPitch, &data.Pixels[l_SourceOffset], l_RowBytes[l_Mip]);
				l_SourceOffset += l_RowBytes[l_Mip];
			}
		}

		l_Staging->Unmap(0, nullptr);

		if (!m_CopyList.Reset(0))
		{
			return false;
		}

		ID3D12GraphicsCommandList* l_List = m_CopyList.GetHandle();
		for (uint32_t l_Mip = 0; l_Mip < data.MipCount; ++l_Mip)
		{
			D3D12_TEXTURE_COPY_LOCATION l_Destination = {};
			l_Destination.pResource = texture.Resource.Get();
			l_Destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			l_Destination.SubresourceIndex = l_Mip;

			D3D12_TEXTURE_COPY_LOCATION l_Source = {};
			l_Source.pResource = l_Staging.Get();
			l_Source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			l_Source.PlacedFootprint = l_Footprints[l_Mip];

			l_List->CopyTextureRegion(&l_Destination, 0, 0, 0, &l_Source, nullptr);
		}

		if (!m_CopyList.Close())
		{
			return false;
		}

		m_CopyQueue->WaitForFence(m_CopyQueue->ExecuteCommandList(l_List));

		D3D12_SHADER_RESOURCE_VIEW_DESC l_View = {};
		l_View.Format = l_Info.Format;
		l_View.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		l_View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		l_View.Texture2D.MostDetailedMip = 0;
		l_View.Texture2D.MipLevels = data.MipCount;
		l_View.Texture2D.PlaneSlice = 0;
		l_View.Texture2D.ResourceMinLODClamp = 0.0f;

		texture.View = m_ResourceHeap->Allocate();
		if (!texture.View.IsValid())
		{
			return false;
		}

		l_Device->CreateShaderResourceView(texture.Resource.Get(), &l_View, texture.View.CPU);

		texture.Width = data.Width;
		texture.Height = data.Height;
		texture.MipCount = data.MipCount;
		texture.Format = l_Info.Format;

		const D3D12_RESOURCE_ALLOCATION_INFO l_AllocationInfo = l_Device->GetResourceAllocationInfo(0, 1, &l_Description);
		gpuBytes = l_AllocationInfo.SizeInBytes;

		return true;
	}
}
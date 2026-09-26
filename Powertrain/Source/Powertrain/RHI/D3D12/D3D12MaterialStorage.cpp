#include "Powertrain/RHI/D3D12/D3D12MaterialStorage.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12TextureStorage.hpp"

namespace Powertrain
{
	bool D3D12MaterialStorage::Initialize()
	{
		m_Slots.clear();
		m_FreeSlots.clear();
		m_AliveCount = 0;
		m_Initialized = true;

		MaterialDescription l_Default;
		l_Default.BaseColor = { 0.75f, 0.75f, 0.75f, 1.0f };
		l_Default.Metallic = 0.0f;
		l_Default.Roughness = 0.8f;

		const MaterialHandle l_Handle = Create(l_Default);
		PT_CORE_ASSERT(l_Handle.Index == k_DefaultMaterial, "The default material must take slot {}", k_DefaultMaterial);

		PT_CORE_INFO("Material storage ready");

		return l_Handle.IsValid();
	}

	void D3D12MaterialStorage::Shutdown()
	{
		if (m_Initialized && m_AliveCount > 1)
		{
			PT_CORE_WARN("Material storage shut down with {} materials still alive", m_AliveCount - 1);
		}

		m_Slots.clear();
		m_FreeSlots.clear();
		m_AliveCount = 0;

		if (m_Initialized)
		{
			PT_CORE_INFO("Material storage shut down");
		}

		m_Initialized = false;
	}

	MaterialHandle D3D12MaterialStorage::Create(const MaterialDescription& description)
	{
		if (!m_Initialized)
		{
			return MaterialHandle();
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
		l_Slot.Description = description;
		l_Slot.Alive = true;
		++m_AliveCount;

		return MaterialHandle{ l_SlotIndex, l_Slot.Generation };
	}

	bool D3D12MaterialStorage::Update(MaterialHandle handle, const MaterialDescription& description)
	{
		uint32_t l_SlotIndex;
		if (!Resolve(handle, l_SlotIndex))
		{
			PT_CORE_WARN("UpdateMaterial on a stale or invalid handle ({}, generation {})", handle.Index, handle.Generation);

			return false;
		}

		m_Slots[l_SlotIndex].Description = description;

		return true;
	}

	void D3D12MaterialStorage::Destroy(MaterialHandle handle)
	{
		uint32_t l_SlotIndex;
		if (!Resolve(handle, l_SlotIndex))
		{
			return;
		}

		if (l_SlotIndex == k_DefaultMaterial)
		{
			PT_CORE_WARN("Ignoring DestroyMaterial on the default material");

			return;
		}

		Slot& l_Slot = m_Slots[l_SlotIndex];
		l_Slot.Description = MaterialDescription();
		l_Slot.Alive = false;
		++l_Slot.Generation;
		--m_AliveCount;

		m_FreeSlots.push_back(l_SlotIndex);
	}

	uint32_t D3D12MaterialStorage::GetSlot(MaterialHandle handle) const
	{
		uint32_t l_SlotIndex;

		return Resolve(handle, l_SlotIndex) ? l_SlotIndex : k_DefaultMaterial;
	}

	void D3D12MaterialStorage::WriteTable(const D3D12TextureStorage& textures, std::span<ShaderInterop::MaterialData> table) const
	{
		PT_CORE_ASSERT(table.size() >= m_Slots.size(), "Material table has {} entries for {} slots", table.size(), m_Slots.size());

		const TextureHandle l_White = textures.GetWhiteTexture();
		const TextureHandle l_FlatNormal = textures.GetFlatNormalTexture();

		for (size_t l_Index = 0; l_Index < m_Slots.size(); ++l_Index)
		{
			const MaterialDescription& l_Source = m_Slots[l_Index].Alive ? m_Slots[l_Index].Description : m_Slots[k_DefaultMaterial].Description;
			ShaderInterop::MaterialData& l_Data = table[l_Index];

			l_Data.BaseColor = { l_Source.BaseColor.R, l_Source.BaseColor.G, l_Source.BaseColor.B, l_Source.BaseColor.A };
			l_Data.Emissive = { l_Source.Emissive.R, l_Source.Emissive.G, l_Source.Emissive.B, 0.0f };
			l_Data.Metallic = l_Source.Metallic;
			l_Data.Roughness = l_Source.Roughness;
			l_Data.AlphaCutoff = l_Source.AlphaCutoff;
			l_Data.Flags = l_Source.Alpha == AlphaMode::Mask ? ShaderInterop::k_MaterialFlagAlphaMask : 0u;
			l_Data.BaseColorTexture = textures.GetDescriptorIndex(l_Source.BaseColorTexture, l_White);
			l_Data.MetallicRoughnessTexture = textures.GetDescriptorIndex(l_Source.MetallicRoughnessTexture, l_White);
			l_Data.NormalTexture = textures.GetDescriptorIndex(l_Source.NormalTexture, l_FlatNormal);
			l_Data.EmissiveTexture = textures.GetDescriptorIndex(l_Source.EmissiveTexture, l_White);
		}
	}

	bool D3D12MaterialStorage::Resolve(MaterialHandle handle, uint32_t& slotIndex) const
	{
		if (!m_Initialized || !handle.IsValid() || handle.Index >= m_Slots.size())
		{
			return false;
		}

		const Slot& l_Slot = m_Slots[handle.Index];
		if (!l_Slot.Alive || l_Slot.Generation != handle.Generation)
		{
			return false;
		}

		slotIndex = handle.Index;

		return true;
	}
}
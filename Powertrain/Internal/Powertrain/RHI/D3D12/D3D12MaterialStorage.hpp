#pragma once

#include "Powertrain/Core/Handle.hpp"
#include "Powertrain/Renderer/RenderTypes.hpp"
#include "Powertrain/Renderer/ShaderInterop.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace Powertrain
{
	class D3D12TextureStorage;

	class D3D12MaterialStorage
	{
	public:
		static constexpr uint32_t k_DefaultMaterial = 0;

		D3D12MaterialStorage() = default;
		~D3D12MaterialStorage() = default;

		D3D12MaterialStorage(const D3D12MaterialStorage&) = delete;
		D3D12MaterialStorage& operator=(const D3D12MaterialStorage&) = delete;

		bool Initialize();
		void Shutdown();

		MaterialHandle Create(const MaterialDescription& description);
		bool Update(MaterialHandle handle, const MaterialDescription& description);
		void Destroy(MaterialHandle handle);

		uint32_t GetSlot(MaterialHandle handle) const;

		void WriteTable(const D3D12TextureStorage& textures, std::span<ShaderInterop::MaterialData> table) const;

		uint32_t GetSlotCount() const { return static_cast<uint32_t>(m_Slots.size()); }
		uint32_t GetAliveCount() const { return m_AliveCount; }

	private:
		struct Slot
		{
			MaterialDescription Description;
			uint32_t Generation = 1;
			bool Alive = false;
		};

		bool Resolve(MaterialHandle handle, uint32_t& slotIndex) const;

	private:
		std::vector<Slot> m_Slots;
		std::vector<uint32_t> m_FreeSlots;

		uint32_t m_AliveCount = 0;
		bool m_Initialized = false;
	};
}
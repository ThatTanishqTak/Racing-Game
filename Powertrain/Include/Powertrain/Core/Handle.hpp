#pragma once

#include <cstdint>

namespace Powertrain
{
	template<typename Tag>
	struct Handle
	{
		uint32_t Index = UINT32_MAX;
		uint32_t Generation = 0;

		bool IsValid() const { return Index != UINT32_MAX; }
		bool operator==(const Handle&) const = default;
	};

	using MeshHandle = Handle<struct MeshTag>;
	using ModelHandle = Handle<struct ModelTag>;
	using TextureHandle = Handle<struct TextureTag>;
	using MaterialHandle = Handle<struct MaterialTag>;
	using SoundHandle = Handle<struct SoundTag>;
	using VoiceHandle = Handle<struct VoiceTag>;
}
#pragma once

#include <cstdint>

namespace Powertrain
{
	// Index into the registry plus the generation it was issued with; a destroyed index bumps the generation, so stale handles fail IsAlive
	struct Entity
	{
		static constexpr uint32_t k_InvalidIndex = UINT32_MAX;

		uint32_t Index = k_InvalidIndex;
		uint32_t Generation = 0;

		static constexpr Entity Null() { return {}; }

		constexpr bool IsValid() const { return Index != k_InvalidIndex; }
		constexpr bool operator==(const Entity&) const = default;
	};
}
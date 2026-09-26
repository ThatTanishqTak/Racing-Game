#pragma once

#include "Powertrain/Renderer/RenderTypes.hpp"

#include <cstdint>

namespace Powertrain
{
	namespace TexturePrimitives
	{
		TextureData MakeSolid(const Color& color, TextureFormat format = TextureFormat::RGBA8Unorm);
		TextureData MakeChecker(uint32_t size, uint32_t cellSize, const Color& colorA, const Color& colorB);
	}
}
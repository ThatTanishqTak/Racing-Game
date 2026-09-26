#include "Powertrain/Renderer/TexturePrimitives.hpp"

#include "Powertrain/Core/CoreLog.hpp"

#include <algorithm>
#include <cmath>

namespace Powertrain
{
	namespace
	{
		float EncodeSrgb(float linear)
		{
			const float l_Clamped = std::clamp(linear, 0.0f, 1.0f);

			return l_Clamped <= 0.0031308f ? l_Clamped * 12.92f : 1.055f * std::pow(l_Clamped, 1.0f / 2.4f) - 0.055f;
		}

		uint8_t ToByte(float value)
		{
			return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
		}

		void WritePixel(uint8_t* destination, const Color& color, bool srgb)
		{
			destination[0] = ToByte(srgb ? EncodeSrgb(color.R) : color.R);
			destination[1] = ToByte(srgb ? EncodeSrgb(color.G) : color.G);
			destination[2] = ToByte(srgb ? EncodeSrgb(color.B) : color.B);
			destination[3] = ToByte(color.A);
		}
	}

	namespace TexturePrimitives
	{
		TextureData MakeSolid(const Color& color, TextureFormat format)
		{
			PT_CORE_ASSERT(format == TextureFormat::RGBA8Unorm || format == TextureFormat::RGBA8Srgb, "MakeSolid writes 8-bit pixels only");

			TextureData l_Texture;
			l_Texture.Width = 1;
			l_Texture.Height = 1;
			l_Texture.MipCount = 1;
			l_Texture.Format = format;
			l_Texture.Pixels.resize(4);

			WritePixel(l_Texture.Pixels.data(), color, format == TextureFormat::RGBA8Srgb);

			return l_Texture;
		}

		TextureData MakeChecker(uint32_t size, uint32_t cellSize, const Color& colorA, const Color& colorB)
		{
			PT_CORE_ASSERT(size > 0 && cellSize > 0, "Checker needs a positive size and cell size, got {} and {}", size, cellSize);

			TextureData l_Texture;
			l_Texture.Width = size;
			l_Texture.Height = size;
			l_Texture.MipCount = 1;
			l_Texture.Format = TextureFormat::RGBA8Srgb;
			l_Texture.Pixels.resize(static_cast<size_t>(size) * size * 4);

			for (uint32_t l_Y = 0; l_Y < size; ++l_Y)
			{
				for (uint32_t l_X = 0; l_X < size; ++l_X)
				{
					const bool l_Even = ((l_X / cellSize) + (l_Y / cellSize)) % 2 == 0;
					WritePixel(&l_Texture.Pixels[(static_cast<size_t>(l_Y) * size + l_X) * 4], l_Even ? colorA : colorB, true);
				}
			}

			return l_Texture;
		}
	}
}
#pragma once

#include <cstdint>

namespace Powertrain
{
	enum class GamepadButton : uint8_t
	{
		A,
		B,
		X,
		Y,
		LeftBumper,
		RightBumper,
		Back,
		Start,
		LeftThumb,
		RightThumb,
		DpadUp,
		DpadDown,
		DpadLeft,
		DpadRight,

		Count
	};

	enum class GamepadAxis : uint8_t
	{
		LeftStickX,
		LeftStickY,
		RightStickX,
		RightStickY,
		LeftTrigger,
		RightTrigger,

		Count
	};
}
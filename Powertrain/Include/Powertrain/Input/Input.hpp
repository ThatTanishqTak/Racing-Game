#pragma once

#include "Powertrain/Input/GamepadCode.hpp"
#include "Powertrain/Input/KeyCode.hpp"
#include "Powertrain/Math/Vector2.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace Powertrain
{
	using ActionID = uint32_t;

	struct ActionBinding
	{
		std::vector<KeyCode> PositiveKeys;
		std::vector<KeyCode> NegativeKeys;
		std::optional<GamepadAxis> Axis;
		std::optional<GamepadButton> Button;
		uint32_t GamepadIndex = 0;
		float DeadZone = 0.1f;
		bool Invert = false;
	};

	class Input
	{
	public:
		virtual ~Input() = default;

		// Keyboard
		virtual bool IsKeyDown(KeyCode key) const = 0;
		virtual bool WasKeyPressed(KeyCode key) const = 0;
		virtual bool WasKeyReleased(KeyCode key) const = 0;

		// Mouse
		virtual bool IsMouseButtonDown(MouseButton button) const = 0;
		virtual bool WasMouseButtonPressed(MouseButton button) const = 0;
		virtual Vector2 GetMouseDelta() const = 0;
		virtual float GetMouseScroll() const = 0;

		// Gamepad
		virtual bool IsGamepadConnected(uint32_t index) const = 0;
		virtual bool IsGamepadButtonDown(uint32_t index, GamepadButton button) const = 0;
		virtual bool WasGamepadButtonPressed(uint32_t index, GamepadButton button) const = 0;
		virtual float GetGamepadAxis(uint32_t index, GamepadAxis axis) const = 0;
		virtual void SetGamepadRumble(uint32_t index, float lowFrequency, float highFrequency) = 0;

		virtual void MapAction(ActionID action, const ActionBinding& binding) = 0;
		virtual float GetActionValue(ActionID action) const = 0;
		virtual bool IsActionDown(ActionID action) const = 0;
		virtual bool WasActionPressed(ActionID action) const = 0;
		virtual bool WasActionReleased(ActionID action) const = 0;
	};
}
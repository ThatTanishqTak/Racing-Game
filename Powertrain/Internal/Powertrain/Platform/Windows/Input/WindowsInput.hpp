#pragma once

#include "Powertrain/Input/Input.hpp"
#include "Powertrain/Platform/Windows/WindowsWindow.hpp"

#include <array>
#include <cstdint>
#include <unordered_map>

namespace Powertrain
{
	class WindowsInput final : public Input
	{
	public:
		static constexpr uint32_t k_MaxGamepads = 4;

		bool Initialize(WindowsWindow& window, EventCallback callback);
		void Shutdown();

		void BeginFrame();
		void Update(double frameTime);
		void BeginFixedPhase();
		void EndFixedStep();
		void EndFixedPhase();

		bool IsKeyDown(KeyCode key) const override;
		bool WasKeyPressed(KeyCode key) const override;
		bool WasKeyReleased(KeyCode key) const override;

		bool IsMouseButtonDown(MouseButton button) const override;
		bool WasMouseButtonPressed(MouseButton button) const override;
		Vector2 GetMouseDelta() const override { return m_MouseDelta; }
		float GetMouseScroll() const override { return m_MouseScroll; }

		bool IsGamepadConnected(uint32_t index) const override;
		bool IsGamepadButtonDown(uint32_t index, GamepadButton button) const override;
		bool WasGamepadButtonPressed(uint32_t index, GamepadButton button) const override;
		float GetGamepadAxis(uint32_t index, GamepadAxis axis) const override;
		void SetGamepadRumble(uint32_t index, float lowFrequency, float highFrequency) override;

		void MapAction(ActionID action, const ActionBinding& binding) override;
		float GetActionValue(ActionID action) const override;
		bool IsActionDown(ActionID action) const override;
		bool WasActionPressed(ActionID action) const override;
		bool WasActionReleased(ActionID action) const override;

	private:
		struct ButtonState
		{
			bool Down = false;
			bool FramePressed = false;
			bool FrameReleased = false;
			bool FixedPressed = false;
			bool FixedReleased = false;
		};

		struct GamepadState
		{
			bool Connected = false;
			double RetryTimer = 0.0;
			std::array<ButtonState, static_cast<size_t>(GamepadButton::Count)> Buttons = {};
			std::array<float, static_cast<size_t>(GamepadAxis::Count)> Axes = {};
		};

		struct ActionState
		{
			ActionBinding Binding;
			float Value = 0.0f;
			ButtonState Down;
		};

		bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
		void OnMouseButton(MouseButton button, bool pressed);
		void ClearState();
		void UpdateActions();
		void Emit(const EventData& data);

		template<typename Function>
		void ForEachButton(Function&& function);

		bool WasPressed(const ButtonState& state) const { return m_FixedPhase ? state.FixedPressed : state.FramePressed; }
		bool WasReleased(const ButtonState& state) const { return m_FixedPhase ? state.FixedReleased : state.FrameReleased; }

		static void Press(ButtonState& state);
		static void Release(ButtonState& state);

	private:
		EventCallback m_Callback;
		HWND m_WindowHandle = nullptr;

		std::array<ButtonState, static_cast<size_t>(KeyCode::Count)> m_Keys = {};
		std::array<ButtonState, static_cast<size_t>(MouseButton::Count)> m_MouseButtons = {};
		Vector2 m_MouseDelta;
		float m_MouseScroll = 0.0f;

		std::array<GamepadState, k_MaxGamepads> m_Gamepads = {};
		std::unordered_map<ActionID, ActionState> m_Actions;

		bool m_FixedPhase = false;
	};
}
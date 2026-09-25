#include "Powertrain/Platform/Windows/Input/WindowsInput.hpp"

#include "Powertrain/Core/CoreLog.hpp"

#include <Xinput.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace Powertrain
{
	namespace
	{
		// XInputGetState on an empty slot is slow, so disconnected pads are polled once a second
		constexpr double k_GamepadRetryInterval = 1.0;

		// Same order as GamepadButton
		constexpr WORD k_GamepadButtonMasks[] =
		{
			XINPUT_GAMEPAD_A,
			XINPUT_GAMEPAD_B,
			XINPUT_GAMEPAD_X,
			XINPUT_GAMEPAD_Y,
			XINPUT_GAMEPAD_LEFT_SHOULDER,
			XINPUT_GAMEPAD_RIGHT_SHOULDER,
			XINPUT_GAMEPAD_BACK,
			XINPUT_GAMEPAD_START,
			XINPUT_GAMEPAD_LEFT_THUMB,
			XINPUT_GAMEPAD_RIGHT_THUMB,
			XINPUT_GAMEPAD_DPAD_UP,
			XINPUT_GAMEPAD_DPAD_DOWN,
			XINPUT_GAMEPAD_DPAD_LEFT,
			XINPUT_GAMEPAD_DPAD_RIGHT,
		};
		static_assert(std::size(k_GamepadButtonMasks) == static_cast<size_t>(GamepadButton::Count));

		float NormalizeStick(SHORT value)
		{
			return std::clamp(static_cast<float>(value) / 32767.0f, -1.0f, 1.0f);
		}

		float NormalizeTrigger(BYTE value)
		{
			return static_cast<float>(value) / 255.0f;
		}

		float ApplyDeadZone(float value, float deadZone)
		{
			const float l_Magnitude = std::abs(value);
			if (deadZone >= 1.0f || l_Magnitude <= deadZone)
			{
				return 0.0f;
			}

			const float l_Scaled = std::min((l_Magnitude - deadZone) / (1.0f - deadZone), 1.0f);

			return std::copysign(l_Scaled, value);
		}

		// KeyCode mirrors the virtual-key codes, so this only resolves left/right for the modifiers
		KeyCode TranslateKey(WPARAM wParam, LPARAM lParam)
		{
			UINT l_VirtualKey = static_cast<UINT>(wParam);
			const UINT l_ScanCode = (static_cast<UINT>(lParam) >> 16) & 0xFF;
			const bool l_Extended = (lParam & 0x01000000) != 0;

			switch (l_VirtualKey)
			{
				case VK_SHIFT:
				{
					l_VirtualKey = MapVirtualKeyW(l_ScanCode, MAPVK_VSC_TO_VK_EX);

					break;
				}
				case VK_CONTROL:
				{
					l_VirtualKey = l_Extended ? VK_RCONTROL : VK_LCONTROL;

					break;
				}
				case VK_MENU:
				{
					l_VirtualKey = l_Extended ? VK_RMENU : VK_LMENU;

					break;
				}
			}

			if (l_VirtualKey == 0 || l_VirtualKey >= static_cast<UINT>(KeyCode::Count))
			{
				return KeyCode::Unknown;
			}

			return static_cast<KeyCode>(l_VirtualKey);
		}
	}

	bool WindowsInput::Initialize(WindowsWindow& window, EventCallback callback)
	{
		m_Callback = std::move(callback);
		m_WindowHandle = window.GetHandle();

		RAWINPUTDEVICE l_Mouse = {};
		l_Mouse.usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
		l_Mouse.usUsage = 0x02; // HID_USAGE_GENERIC_MOUSE
		l_Mouse.dwFlags = 0; // Foreground only
		l_Mouse.hwndTarget = m_WindowHandle;

		if (!RegisterRawInputDevices(&l_Mouse, 1, sizeof(l_Mouse)))
		{
			PT_CORE_ERROR("RegisterRawInputDevices failed with error {}", GetLastError());

			return false;
		}

		window.AddMessageHandler([this](UINT message, WPARAM wParam, LPARAM lParam)
		{
			return HandleMessage(message, wParam, lParam);
		});

		PT_CORE_INFO("Input initialized");

		return true;
	}

	void WindowsInput::Shutdown()
	{
		for (uint32_t l_Index = 0; l_Index < k_MaxGamepads; ++l_Index)
		{
			if (m_Gamepads[l_Index].Connected)
			{
				SetGamepadRumble(l_Index, 0.0f, 0.0f);
			}
		}

		m_Actions.clear();
		m_Callback = nullptr;
	}

	void WindowsInput::BeginFrame()
	{
		ForEachButton([](ButtonState& state)
		{
			state.FramePressed = false;
			state.FrameReleased = false;
		});

		m_MouseDelta = Vector2{};
		m_MouseScroll = 0.0f;
	}

	void WindowsInput::Update(double frameTime)
	{
		for (uint32_t l_Index = 0; l_Index < k_MaxGamepads; ++l_Index)
		{
			GamepadState& l_Pad = m_Gamepads[l_Index];

			if (!l_Pad.Connected)
			{
				l_Pad.RetryTimer -= frameTime;
				if (l_Pad.RetryTimer > 0.0)
				{
					continue;
				}

				l_Pad.RetryTimer = k_GamepadRetryInterval;
			}

			XINPUT_STATE l_State = {};
			if (XInputGetState(l_Index, &l_State) != ERROR_SUCCESS)
			{
				if (l_Pad.Connected)
				{
					l_Pad = GamepadState{};
					l_Pad.RetryTimer = k_GamepadRetryInterval;

					PT_CORE_INFO("Gamepad {} disconnected", l_Index);
					Emit(GamepadConnectionEvent{ l_Index, false });
				}

				continue;
			}

			if (!l_Pad.Connected)
			{
				l_Pad.Connected = true;

				PT_CORE_INFO("Gamepad {} connected", l_Index);
				Emit(GamepadConnectionEvent{ l_Index, true });
			}

			const XINPUT_GAMEPAD& l_Gamepad = l_State.Gamepad;

			for (size_t l_Button = 0; l_Button < l_Pad.Buttons.size(); ++l_Button)
			{
				if (l_Gamepad.wButtons & k_GamepadButtonMasks[l_Button])
				{
					Press(l_Pad.Buttons[l_Button]);
				}
				else
				{
					Release(l_Pad.Buttons[l_Button]);
				}
			}

			auto l_Axis = [&l_Pad](GamepadAxis axis) -> float&
			{
				return l_Pad.Axes[static_cast<size_t>(axis)];
			};

			l_Axis(GamepadAxis::LeftStickX) = NormalizeStick(l_Gamepad.sThumbLX);
			l_Axis(GamepadAxis::LeftStickY) = NormalizeStick(l_Gamepad.sThumbLY);
			l_Axis(GamepadAxis::RightStickX) = NormalizeStick(l_Gamepad.sThumbRX);
			l_Axis(GamepadAxis::RightStickY) = NormalizeStick(l_Gamepad.sThumbRY);
			l_Axis(GamepadAxis::LeftTrigger) = NormalizeTrigger(l_Gamepad.bLeftTrigger);
			l_Axis(GamepadAxis::RightTrigger) = NormalizeTrigger(l_Gamepad.bRightTrigger);
		}

		UpdateActions();
	}

	void WindowsInput::BeginFixedPhase()
	{
		m_FixedPhase = true;
	}

	void WindowsInput::EndFixedStep()
	{
		ForEachButton([](ButtonState& state)
		{
			state.FixedPressed = false;
			state.FixedReleased = false;
		});
	}

	void WindowsInput::EndFixedPhase()
	{
		m_FixedPhase = false;
	}

	bool WindowsInput::IsKeyDown(KeyCode key) const
	{
		const size_t l_Index = static_cast<size_t>(key);

		return l_Index < m_Keys.size() && m_Keys[l_Index].Down;
	}

	bool WindowsInput::WasKeyPressed(KeyCode key) const
	{
		const size_t l_Index = static_cast<size_t>(key);

		return l_Index < m_Keys.size() && WasPressed(m_Keys[l_Index]);
	}

	bool WindowsInput::WasKeyReleased(KeyCode key) const
	{
		const size_t l_Index = static_cast<size_t>(key);

		return l_Index < m_Keys.size() && WasReleased(m_Keys[l_Index]);
	}

	bool WindowsInput::IsMouseButtonDown(MouseButton button) const
	{
		const size_t l_Index = static_cast<size_t>(button);

		return l_Index < m_MouseButtons.size() && m_MouseButtons[l_Index].Down;
	}

	bool WindowsInput::WasMouseButtonPressed(MouseButton button) const
	{
		const size_t l_Index = static_cast<size_t>(button);

		return l_Index < m_MouseButtons.size() && WasPressed(m_MouseButtons[l_Index]);
	}

	bool WindowsInput::IsGamepadConnected(uint32_t index) const
	{
		return index < k_MaxGamepads && m_Gamepads[index].Connected;
	}

	bool WindowsInput::IsGamepadButtonDown(uint32_t index, GamepadButton button) const
	{
		return IsGamepadConnected(index) && m_Gamepads[index].Buttons[static_cast<size_t>(button)].Down;
	}

	bool WindowsInput::WasGamepadButtonPressed(uint32_t index, GamepadButton button) const
	{
		return IsGamepadConnected(index) && WasPressed(m_Gamepads[index].Buttons[static_cast<size_t>(button)]);
	}

	float WindowsInput::GetGamepadAxis(uint32_t index, GamepadAxis axis) const
	{
		return IsGamepadConnected(index) ? m_Gamepads[index].Axes[static_cast<size_t>(axis)] : 0.0f;
	}

	void WindowsInput::SetGamepadRumble(uint32_t index, float lowFrequency, float highFrequency)
	{
		if (index >= k_MaxGamepads)
		{
			return;
		}

		XINPUT_VIBRATION l_Vibration = {};
		l_Vibration.wLeftMotorSpeed = static_cast<WORD>(std::clamp(lowFrequency, 0.0f, 1.0f) * 65535.0f);
		l_Vibration.wRightMotorSpeed = static_cast<WORD>(std::clamp(highFrequency, 0.0f, 1.0f) * 65535.0f);

		XInputSetState(index, &l_Vibration);
	}

	void WindowsInput::MapAction(ActionID action, const ActionBinding& binding)
	{
		ActionState& l_Action = m_Actions[action];
		l_Action.Binding = binding;
		l_Action.Value = 0.0f;
		l_Action.Down = ButtonState{};
	}

	float WindowsInput::GetActionValue(ActionID action) const
	{
		const auto l_Iterator = m_Actions.find(action);

		return l_Iterator != m_Actions.end() ? l_Iterator->second.Value : 0.0f;
	}

	bool WindowsInput::IsActionDown(ActionID action) const
	{
		const auto l_Iterator = m_Actions.find(action);

		return l_Iterator != m_Actions.end() && l_Iterator->second.Down.Down;
	}

	bool WindowsInput::WasActionPressed(ActionID action) const
	{
		const auto l_Iterator = m_Actions.find(action);

		return l_Iterator != m_Actions.end() && WasPressed(l_Iterator->second.Down);
	}

	bool WindowsInput::WasActionReleased(ActionID action) const
	{
		const auto l_Iterator = m_Actions.find(action);

		return l_Iterator != m_Actions.end() && WasReleased(l_Iterator->second.Down);
	}

	bool WindowsInput::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
			{
				const KeyCode l_Key = TranslateKey(wParam, lParam);
				if (l_Key == KeyCode::Unknown)
				{
					return false;
				}

				const bool l_Repeat = (lParam & 0x40000000) != 0;
				if (!l_Repeat)
				{
					Press(m_Keys[static_cast<size_t>(l_Key)]);
				}

				Emit(KeyEvent{ l_Key, true, l_Repeat });

				return false;
			}
			case WM_KEYUP:
			case WM_SYSKEYUP:
			{
				const KeyCode l_Key = TranslateKey(wParam, lParam);
				if (l_Key == KeyCode::Unknown)
				{
					return false;
				}

				Release(m_Keys[static_cast<size_t>(l_Key)]);
				Emit(KeyEvent{ l_Key, false, false });

				return false;
			}
			case WM_LBUTTONDOWN:
			{
				OnMouseButton(MouseButton::Left, true);

				return false;
			}
			case WM_LBUTTONUP:
			{
				OnMouseButton(MouseButton::Left, false);

				return false;
			}
			case WM_RBUTTONDOWN:
			{
				OnMouseButton(MouseButton::Right, true);

				return false;
			}
			case WM_RBUTTONUP:
			{
				OnMouseButton(MouseButton::Right, false);

				return false;
			}
			case WM_MBUTTONDOWN:
			{
				OnMouseButton(MouseButton::Middle, true);

				return false;
			}
			case WM_MBUTTONUP:
			{
				OnMouseButton(MouseButton::Middle, false);

				return false;
			}
			case WM_XBUTTONDOWN:
			{
				OnMouseButton(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? MouseButton::X1 : MouseButton::X2, true);

				return false;
			}
			case WM_XBUTTONUP:
			{
				OnMouseButton(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? MouseButton::X1 : MouseButton::X2, false);

				return false;
			}
			case WM_MOUSEWHEEL:
			{
				const float l_Delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
				m_MouseScroll += l_Delta;
				Emit(MouseScrollEvent{ l_Delta });

				return false;
			}
			case WM_INPUT:
			{
				RAWINPUT l_Raw = {};
				UINT l_Size = sizeof(l_Raw);
				if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &l_Raw, &l_Size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))
				{
					return false;
				}

				if (l_Raw.header.dwType == RIM_TYPEMOUSE && (l_Raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
				{
					m_MouseDelta.X += static_cast<float>(l_Raw.data.mouse.lLastX);
					m_MouseDelta.Y += static_cast<float>(l_Raw.data.mouse.lLastY);
				}

				return false;
			}
			case WM_KILLFOCUS:
			{
				ClearState();

				return false;
			}
		}

		return false;
	}

	void WindowsInput::OnMouseButton(MouseButton button, bool pressed)
	{
		ButtonState& l_State = m_MouseButtons[static_cast<size_t>(button)];

		const auto l_AnyDown = [this]()
		{
			return std::any_of(m_MouseButtons.begin(), m_MouseButtons.end(), [](const ButtonState& state) { return state.Down; });
		};

		if (pressed)
		{
			// Capture so drags that leave the client area still deliver the button-up.
			if (!l_AnyDown() && m_WindowHandle)
			{
				SetCapture(m_WindowHandle);
			}

			Press(l_State);
		}
		else
		{
			Release(l_State);

			if (!l_AnyDown())
			{
				ReleaseCapture();
			}
		}

		Emit(MouseButtonEvent{ button, pressed });
	}

	void WindowsInput::ClearState()
	{
		for (ButtonState& l_State : m_Keys)
		{
			Release(l_State);
		}

		for (ButtonState& l_State : m_MouseButtons)
		{
			Release(l_State);
		}

		ReleaseCapture();
	}

	void WindowsInput::UpdateActions()
	{
		for (auto& [l_ID, l_Action] : m_Actions)
		{
			const ActionBinding& l_Binding = l_Action.Binding;

			float l_KeyValue = 0.0f;
			if (std::any_of(l_Binding.PositiveKeys.begin(), l_Binding.PositiveKeys.end(), [this](KeyCode key) { return IsKeyDown(key); }))
			{
				l_KeyValue += 1.0f;
			}
			if (std::any_of(l_Binding.NegativeKeys.begin(), l_Binding.NegativeKeys.end(), [this](KeyCode key) { return IsKeyDown(key); }))
			{
				l_KeyValue -= 1.0f;
			}

			float l_PadValue = 0.0f;
			if (IsGamepadConnected(l_Binding.GamepadIndex))
			{
				const GamepadState& l_Pad = m_Gamepads[l_Binding.GamepadIndex];

				if (l_Binding.Axis)
				{
					float l_AxisValue = ApplyDeadZone(l_Pad.Axes[static_cast<size_t>(*l_Binding.Axis)], l_Binding.DeadZone);
					if (l_Binding.Invert)
					{
						l_AxisValue = -l_AxisValue;
					}

					l_PadValue = l_AxisValue;
				}

				if (l_Binding.Button && l_Pad.Buttons[static_cast<size_t>(*l_Binding.Button)].Down && std::abs(l_PadValue) < 1.0f)
				{
					l_PadValue = 1.0f;
				}
			}

			// Whichever device is pushed harder wins, so a resting stick never cancels the keyboard.
			l_Action.Value = std::abs(l_KeyValue) >= std::abs(l_PadValue) ? l_KeyValue : l_PadValue;

			if (std::abs(l_Action.Value) > 0.5f)
			{
				Press(l_Action.Down);
			}
			else
			{
				Release(l_Action.Down);
			}
		}
	}

	void WindowsInput::Emit(const EventData& data)
	{
		if (m_Callback)
		{
			Event l_Event(data);
			m_Callback(l_Event);
		}
	}

	template<typename Function>
	void WindowsInput::ForEachButton(Function&& function)
	{
		for (ButtonState& l_State : m_Keys)
		{
			function(l_State);
		}

		for (ButtonState& l_State : m_MouseButtons)
		{
			function(l_State);
		}

		for (GamepadState& l_Pad : m_Gamepads)
		{
			for (ButtonState& l_State : l_Pad.Buttons)
			{
				function(l_State);
			}
		}

		for (auto& [l_ID, l_Action] : m_Actions)
		{
			function(l_Action.Down);
		}
	}

	void WindowsInput::Press(ButtonState& state)
	{
		if (!state.Down)
		{
			state.Down = true;
			state.FramePressed = true;
			state.FixedPressed = true;
		}
	}

	void WindowsInput::Release(ButtonState& state)
	{
		if (state.Down)
		{
			state.Down = false;
			state.FrameReleased = true;
			state.FixedReleased = true;
		}
	}
}
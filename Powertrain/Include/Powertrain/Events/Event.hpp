#pragma once

#include "Powertrain/Input/KeyCode.hpp"

#include <cstdint>
#include <variant>

namespace Powertrain
{
	struct WindowCloseEvent {};
	struct WindowResizeEvent { uint32_t Width = 0; uint32_t Height = 0; };
	struct WindowMinimizeEvent { bool Minimized = false; };
	struct WindowFocusEvent { bool Focused = false; };
	struct KeyEvent { KeyCode Key = KeyCode::Unknown; bool Pressed = false; bool Repeat = false; };
	struct MouseButtonEvent { MouseButton Button = MouseButton::Left; bool Pressed = false; };
	struct MouseScrollEvent { float Delta = 0.0f; };
	struct GamepadConnectionEvent { uint32_t Index = 0; bool Connected = false; };

	using EventData = std::variant<WindowCloseEvent, WindowResizeEvent, WindowMinimizeEvent, WindowFocusEvent, KeyEvent, MouseButtonEvent, MouseScrollEvent, GamepadConnectionEvent>;

	class Event
	{
	public:
		explicit Event(const EventData& data) : m_Data(data) {}

		template<typename T, typename Function>
		bool Dispatch(Function&& function)
		{
			if (m_Handled)
			{
				return false;
			}

			if (T* l_Payload = std::get_if<T>(&m_Data))
			{
				m_Handled = function(*l_Payload);

				return true;
			}

			return false;
		}

		bool IsHandled() const { return m_Handled; }
		const EventData& GetData() const { return m_Data; }

	private:
		EventData m_Data;
		bool m_Handled = false;
	};
}
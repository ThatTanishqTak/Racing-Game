#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Powertrain
{
	enum class WindowMode : uint8_t
	{
		Windowed,
		BorderlessFullscreen
	};

	struct WindowProperties
	{
		std::string Title = "Powertrain-Window";
		
		uint32_t Width = 1280;
		uint32_t Height = 720;

		WindowMode Mode = WindowMode::Windowed;
		bool Resizable = true;
	};

	class Window
	{
	public:
		virtual ~Window() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual WindowMode GetMode() const = 0;

		virtual bool IsMinimized() const = 0;
		virtual bool IsFocused() const = 0;
		virtual bool IsCursorLocked() const = 0;

		virtual void SetTitle(std::string_view title) = 0;
		virtual void SetMode(WindowMode mode) = 0;
		virtual void SetCursorLocked(bool locked) = 0;
	};
}
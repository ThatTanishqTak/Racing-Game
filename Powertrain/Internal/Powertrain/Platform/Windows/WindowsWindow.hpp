#pragma once

#include "Powertrain/Events/Event.hpp"
#include "Powertrain/Platform/Windows/Win32.hpp"
#include "Powertrain/Window/Window.hpp"

#include <functional>
#include <vector>

namespace Powertrain
{
	using EventCallback = std::function<void(Event&)>;
	using MessageHandler = std::function<bool(UINT message, WPARAM wParam, LPARAM lParam)>;

	class WindowsWindow final : public Window
	{
	public:
		WindowsWindow() = default;
		~WindowsWindow() override;

		WindowsWindow(const WindowsWindow&) = delete;
		WindowsWindow& operator=(const WindowsWindow&) = delete;

		bool Initialize(const WindowProperties& properties, EventCallback callback);
		void Shutdown();

		void PollEvents();
		void WaitForMessages();

		// Handlers run before the window's own handling. Returning true swallows the message.
		void AddMessageHandler(MessageHandler handler);
		LRESULT HandleMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

		HWND GetHandle() const { return m_WindowHandle; }

		uint32_t GetWidth() const override { return m_Width; }
		uint32_t GetHeight() const override { return m_Height; }
		WindowMode GetMode() const override { return m_Mode; }
		bool IsMinimized() const override { return m_Minimized; }
		bool IsFocused() const override { return m_Focused; }
		bool IsCursorLocked() const override { return m_CursorLocked; }

		void SetTitle(std::string_view title) override;
		void SetMode(WindowMode mode) override;
		void SetCursorLocked(bool locked) override;

	private:
		void Emit(const EventData& data);
		void FlushResize();
		void ApplyCursorLock();

	private:
		EventCallback m_Callback;
		std::vector<MessageHandler> m_MessageHandlers;

		HWND m_WindowHandle = nullptr;
		ATOM m_ClassAtom = 0;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		WindowMode m_Mode = WindowMode::Windowed;
		bool m_Resizable = true;
		bool m_Minimized = false;
		bool m_Focused = false;
		bool m_InSizeMove = false;
		bool m_ResizePending = false;

		bool m_CursorLocked = false;
		bool m_CursorHidden = false;

		RECT m_WindowedRect = {};
	};
}
#include "Powertrain/Platform/Windows/WindowsWindow.hpp"

#include "Powertrain/Core/CoreLog.hpp"

#include <utility>

namespace Powertrain
{
	namespace
	{
		constexpr const wchar_t* k_ClassName = L"PowertrainWindow";

		LRESULT CALLBACK WindowProcedure(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
		{
			if (message == WM_NCCREATE)
			{
				const CREATESTRUCTW* l_CreateStruct = reinterpret_cast<const CREATESTRUCTW*>(lParam);
				SetWindowLongPtrW(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(l_CreateStruct->lpCreateParams));
			}

			WindowsWindow* l_Window = reinterpret_cast<WindowsWindow*>(GetWindowLongPtrW(windowHandle, GWLP_USERDATA));
			if (l_Window)
			{
				return l_Window->HandleMessage(windowHandle, message, wParam, lParam);
			}

			return DefWindowProcW(windowHandle, message, wParam, lParam);
		}

		DWORD GetWindowedStyle(bool resizable)
		{
			DWORD l_Style = WS_OVERLAPPEDWINDOW;
			if (!resizable)
			{
				l_Style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
			}

			return l_Style;
		}
	}

	WindowsWindow::~WindowsWindow()
	{
		Shutdown();
	}

	bool WindowsWindow::Initialize(const WindowProperties& properties, EventCallback callback)
	{
		m_Callback = std::move(callback);
		m_Resizable = properties.Resizable;

		// Must happen before any window is created. Fails harmlessly if a manifest already set it.
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

		const HINSTANCE l_Instance = GetModuleHandleW(nullptr);

		WNDCLASSEXW l_Class = {};
		l_Class.cbSize = sizeof(l_Class);
		l_Class.lpfnWndProc = WindowProcedure;
		l_Class.hInstance = l_Instance;
		l_Class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		l_Class.hbrBackground = nullptr;
		l_Class.lpszClassName = k_ClassName;

		m_ClassAtom = RegisterClassExW(&l_Class);
		if (m_ClassAtom == 0)
		{
			PT_CORE_ERROR("RegisterClassExW failed with error {}", GetLastError());

			return false;
		}

		const DWORD l_Style = GetWindowedStyle(m_Resizable);
		const DWORD l_ExStyle = WS_EX_APPWINDOW;

		RECT l_Rect = { 0, 0, static_cast<LONG>(properties.Width), static_cast<LONG>(properties.Height) };
		AdjustWindowRectExForDpi(&l_Rect, l_Style, FALSE, l_ExStyle, GetDpiForSystem());

		const std::wstring l_Title = Win32::ToWide(properties.Title);
		m_WindowHandle = CreateWindowExW(l_ExStyle, k_ClassName, l_Title.c_str(), l_Style, CW_USEDEFAULT, CW_USEDEFAULT, l_Rect.right - l_Rect.left, l_Rect.bottom - l_Rect.top, nullptr, nullptr, l_Instance, this);
		if (!m_WindowHandle)
		{
			PT_CORE_ERROR("CreateWindowExW failed with error {}", GetLastError());

			return false;
		}

		ShowWindow(m_WindowHandle, SW_SHOW);

		if (properties.Mode == WindowMode::BorderlessFullscreen)
		{
			SetMode(WindowMode::BorderlessFullscreen);
		}

		PT_CORE_INFO("Window created: {}x{}", m_Width, m_Height);

		return true;
	}

	void WindowsWindow::Shutdown()
	{
		if (m_WindowHandle)
		{
			m_CursorLocked = false;
			ApplyCursorLock();

			DestroyWindow(m_WindowHandle);
			m_WindowHandle = nullptr;
		}

		if (m_ClassAtom != 0)
		{
			UnregisterClassW(k_ClassName, GetModuleHandleW(nullptr));
			m_ClassAtom = 0;
		}

		m_MessageHandlers.clear();
		m_Callback = nullptr;
	}

	void WindowsWindow::PollEvents()
	{
		MSG l_Message;
		while (PeekMessageW(&l_Message, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&l_Message);
			DispatchMessageW(&l_Message);
		}
	}

	void WindowsWindow::WaitForMessages()
	{
		WaitMessage();
	}

	void WindowsWindow::AddMessageHandler(MessageHandler handler)
	{
		m_MessageHandlers.push_back(std::move(handler));
	}

	LRESULT WindowsWindow::HandleMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (!m_WindowHandle)
		{
			// Messages sent from inside CreateWindowExW arrive before the handle is returned.
			m_WindowHandle = windowHandle;
		}

		for (const MessageHandler& l_Handler : m_MessageHandlers)
		{
			if (l_Handler(message, wParam, lParam))
			{
				return 0;
			}
		}

		switch (message)
		{
			case WM_CLOSE:
			{
				Emit(WindowCloseEvent{});

				return 0;
			}
			case WM_SIZE:
			{
				if (wParam == SIZE_MINIMIZED)
				{
					if (!m_Minimized)
					{
						m_Minimized = true;
						Emit(WindowMinimizeEvent{ true });
					}

					return 0;
				}

				if (m_Minimized)
				{
					m_Minimized = false;
					Emit(WindowMinimizeEvent{ false });
				}

				const uint32_t l_Width = LOWORD(lParam);
				const uint32_t l_Height = HIWORD(lParam);
				if (l_Width != m_Width || l_Height != m_Height)
				{
					m_Width = l_Width;
					m_Height = l_Height;
					m_ResizePending = true;
				}

				if (!m_InSizeMove)
				{
					FlushResize();
				}

				ApplyCursorLock();

				return 0;
			}
			case WM_MOVE:
			{
				ApplyCursorLock();

				break;
			}
			case WM_ENTERSIZEMOVE:
			{
				m_InSizeMove = true;

				return 0;
			}
			case WM_EXITSIZEMOVE:
			{
				m_InSizeMove = false;
				FlushResize();

				return 0;
			}
			case WM_SETFOCUS:
			{
				m_Focused = true;
				ApplyCursorLock();
				Emit(WindowFocusEvent{ true });

				return 0;
			}
			case WM_KILLFOCUS:
			{
				m_Focused = false;
				ApplyCursorLock();
				Emit(WindowFocusEvent{ false });

				return 0;
			}
			case WM_DPICHANGED:
			{
				const RECT* l_Suggested = reinterpret_cast<const RECT*>(lParam);
				SetWindowPos(windowHandle, nullptr, l_Suggested->left, l_Suggested->top, l_Suggested->right - l_Suggested->left, l_Suggested->bottom - l_Suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);

				return 0;
			}
			case WM_GETMINMAXINFO:
			{
				MINMAXINFO* l_Info = reinterpret_cast<MINMAXINFO*>(lParam);
				l_Info->ptMinTrackSize = POINT{ 320, 240 };

				return 0;
			}
			case WM_SYSCOMMAND:
			{
				// Stop Alt and F10 from activating the (non-existent) menu bar and freezing the loop.
				if ((wParam & 0xFFF0) == SC_KEYMENU)
				{
					return 0;
				}

				break;
			}
			case WM_MENUCHAR:
			{
				// Silence the beep on Alt + key combinations.
				return MAKELRESULT(0, MNC_CLOSE);
			}
			case WM_ERASEBKGND:
			{
				return 1;
			}
			case WM_PAINT:
			{
				ValidateRect(windowHandle, nullptr);

				return 0;
			}
		}

		return DefWindowProcW(windowHandle, message, wParam, lParam);
	}

	void WindowsWindow::SetTitle(std::string_view title)
	{
		if (m_WindowHandle)
		{
			SetWindowTextW(m_WindowHandle, Win32::ToWide(title).c_str());
		}
	}

	void WindowsWindow::SetMode(WindowMode mode)
	{
		if (!m_WindowHandle || mode == m_Mode)
		{
			return;
		}

		if (mode == WindowMode::BorderlessFullscreen)
		{
			GetWindowRect(m_WindowHandle, &m_WindowedRect);

			MONITORINFO l_MonitorInfo = {};
			l_MonitorInfo.cbSize = sizeof(l_MonitorInfo);
			GetMonitorInfoW(MonitorFromWindow(m_WindowHandle, MONITOR_DEFAULTTONEAREST), &l_MonitorInfo);
			const RECT& l_Monitor = l_MonitorInfo.rcMonitor;

			SetWindowLongPtrW(m_WindowHandle, GWL_STYLE, static_cast<LONG_PTR>(WS_POPUP | WS_VISIBLE));
			SetWindowPos(m_WindowHandle, HWND_TOP, l_Monitor.left, l_Monitor.top, l_Monitor.right - l_Monitor.left, l_Monitor.bottom - l_Monitor.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		}
		else
		{
			SetWindowLongPtrW(m_WindowHandle, GWL_STYLE, static_cast<LONG_PTR>(GetWindowedStyle(m_Resizable) | WS_VISIBLE));
			SetWindowPos(m_WindowHandle, HWND_NOTOPMOST, m_WindowedRect.left, m_WindowedRect.top, m_WindowedRect.right - m_WindowedRect.left, m_WindowedRect.bottom - m_WindowedRect.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		}

		m_Mode = mode;

		PT_CORE_INFO("Window mode: {}", mode == WindowMode::Windowed ? "windowed" : "borderless fullscreen");
	}

	void WindowsWindow::SetCursorLocked(bool locked)
	{
		m_CursorLocked = locked;
		ApplyCursorLock();
	}

	void WindowsWindow::Emit(const EventData& data)
	{
		if (m_Callback)
		{
			Event l_Event(data);
			m_Callback(l_Event);
		}
	}

	void WindowsWindow::FlushResize()
	{
		if (m_ResizePending)
		{
			m_ResizePending = false;
			Emit(WindowResizeEvent{ m_Width, m_Height });
		}
	}

	void WindowsWindow::ApplyCursorLock()
	{
		const bool l_Lock = m_CursorLocked && m_Focused && m_WindowHandle != nullptr;

		if (l_Lock)
		{
			RECT l_Rect;
			GetClientRect(m_WindowHandle, &l_Rect);
			MapWindowPoints(m_WindowHandle, nullptr, reinterpret_cast<POINT*>(&l_Rect), 2);
			ClipCursor(&l_Rect);
		}
		else
		{
			ClipCursor(nullptr);
		}

		// ShowCursor is a counter, so only ever move it by one in each direction.
		if (l_Lock && !m_CursorHidden)
		{
			ShowCursor(FALSE);
			m_CursorHidden = true;
		}
		else if (!l_Lock && m_CursorHidden)
		{
			ShowCursor(TRUE);
			m_CursorHidden = false;
		}
	}
}
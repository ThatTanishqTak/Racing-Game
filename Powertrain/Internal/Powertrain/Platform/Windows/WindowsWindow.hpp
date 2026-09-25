#pragma once

#include "Powertrain/Window/Window.hpp"

#include <Windows.h>

namespace Powertrain
{
	class WindowsWindow : public Window
	{
	public:
		~WindowsWindow() override;

		bool OnInitialize(const WindowProperties& properties) override;
		void OnShutdown() override;
		void OnUpdate() override;
		
		bool ShouldClose() const override { return m_ShouldClose; }
		void* GetNativeWindow() const override { return m_WindowHandle; }

	private:

	private:
		bool m_ShouldClose = false;
		HWND m_WindowHandle = nullptr;
	};
}
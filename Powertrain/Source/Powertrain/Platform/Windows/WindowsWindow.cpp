#include "Powertrain/Platform/Windows/WindowsWindow.hpp"

namespace Powertrain
{
	WindowsWindow::~WindowsWindow() = default;

	bool WindowsWindow::OnInitialize(const WindowProperties& properties)
	{
		(void)properties;

		return true;
	}

	void WindowsWindow::OnShutdown()
	{

	}

	void WindowsWindow::OnUpdate()
	{

	}
}
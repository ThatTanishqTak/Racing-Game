#pragma once

namespace Powertrain
{
	class Window;

	class EngineContext
	{
	public:
		explicit EngineContext(Window& window) : m_Window(window) {}

		EngineContext(const EngineContext&) = delete;
		EngineContext& operator=(const EngineContext&) = delete;

		Window& GetWindow() const { return m_Window; }

		void RequestExit() { m_ExitRequested = true; }
		bool IsExitRequested() const { return m_ExitRequested; }

	private:
		Window& m_Window;
		bool m_ExitRequested = false;
	};
}
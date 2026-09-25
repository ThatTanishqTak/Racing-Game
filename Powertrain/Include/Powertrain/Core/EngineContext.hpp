#pragma once

namespace Powertrain
{
	class Window;
	class Input;

	class EngineContext
	{
	public:
		EngineContext(Window& window, Input& input) : m_Window(window), m_Input(input) {}

		EngineContext(const EngineContext&) = delete;
		EngineContext& operator=(const EngineContext&) = delete;

		Window& GetWindow() const { return m_Window; }
		Input& GetInput() const { return m_Input; }

		void RequestExit() { m_ExitRequested = true; }
		bool IsExitRequested() const { return m_ExitRequested; }

	private:
		Window& m_Window;
		Input& m_Input;

		bool m_ExitRequested = false;
	};
}
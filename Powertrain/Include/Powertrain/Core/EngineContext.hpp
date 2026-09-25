#pragma once

namespace Powertrain
{
	class Window;
	class Input;
	class Renderer;

	class EngineContext
	{
	public:
		EngineContext(Window& window, Input& input, Renderer& renderer) : m_Window(window), m_Input(input), m_Renderer(renderer) {}

		EngineContext(const EngineContext&) = delete;
		EngineContext& operator=(const EngineContext&) = delete;

		Window& GetWindow() const { return m_Window; }
		Input& GetInput() const { return m_Input; }
		Renderer& GetRenderer() const { return m_Renderer; }

		void RequestExit() { m_ExitRequested = true; }
		bool IsExitRequested() const { return m_ExitRequested; }

	private:
		Window& m_Window;
		Input& m_Input;
		Renderer& m_Renderer;

		bool m_ExitRequested = false;
	};
}
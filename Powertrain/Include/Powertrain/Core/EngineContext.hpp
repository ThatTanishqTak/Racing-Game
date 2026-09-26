#pragma once

namespace Powertrain
{
	class Window;
	class Input;
	class Renderer;
	class SceneManager;

	class EngineContext
	{
	public:
		EngineContext(Window& window, Input& input, Renderer& renderer, SceneManager& sceneManager) : m_Window(window), m_Input(input), m_Renderer(renderer), m_SceneManager(sceneManager) {}

		EngineContext(const EngineContext&) = delete;
		EngineContext& operator=(const EngineContext&) = delete;

		Window& GetWindow() const { return m_Window; }
		Input& GetInput() const { return m_Input; }
		Renderer& GetRenderer() const { return m_Renderer; }
		SceneManager& GetSceneManager() const { return m_SceneManager; }

		void RequestExit() { m_ExitRequested = true; }
		bool IsExitRequested() const { return m_ExitRequested; }

	private:
		Window& m_Window;
		Input& m_Input;
		Renderer& m_Renderer;
		SceneManager& m_SceneManager;

		bool m_ExitRequested = false;
	};
}
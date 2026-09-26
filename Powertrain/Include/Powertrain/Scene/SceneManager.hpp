#pragma once

#include "Powertrain/Core/Timestep.hpp"
#include "Powertrain/Scene/Scene.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Powertrain
{
	class EngineContext;

	class SceneManager
	{
	public:
		SceneManager() = default;
		~SceneManager();

		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;

		Scene& CreateScene(std::string name, EngineContext& context);
		void DestroyScene(Scene& scene);

		void SetActiveScene(Scene& scene);
		void ClearActiveScene();
		Scene* GetActiveScene() const { return m_ActiveScene; }

		size_t GetSceneCount() const { return m_Scenes.size(); }

	private:
		friend class ApplicationImplementation;

		void FixedUpdate(Timestep fixedStep);
		void Update(Timestep deltaTime);
		void SetInterpolationAlpha(float alpha);
		void FlushDeferredChanges();

		void ApplyActiveScene(Scene* scene);
		void ApplyDestroy(Scene& scene);
		bool Owns(const Scene* scene) const;

	private:
		std::vector<std::unique_ptr<Scene>> m_Scenes;
		Scene* m_ActiveScene = nullptr;
		Scene* m_PendingActiveScene = nullptr;
		bool m_ActiveScenePending = false;
		std::vector<Scene*> m_PendingDestroy;
		bool m_Updating = false;
	};
}
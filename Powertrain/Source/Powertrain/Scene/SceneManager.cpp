#include "Powertrain/Scene/SceneManager.hpp"

#include "Powertrain/Core/CoreLog.hpp"

#include <algorithm>

namespace Powertrain
{
	SceneManager::~SceneManager()
	{
		m_ActiveScene = nullptr;
		m_Scenes.clear();
	}

	Scene& SceneManager::CreateScene(std::string name, EngineContext& context)
	{
		m_Scenes.push_back(std::make_unique<Scene>(std::move(name), context));

		return *m_Scenes.back();
	}

	void SceneManager::DestroyScene(Scene& scene)
	{
		PT_CORE_ASSERT(Owns(&scene), "Scene '{}' does not belong to this manager", scene.GetName());

		if (m_Updating)
		{
			m_PendingDestroy.push_back(&scene);

			return;
		}

		ApplyDestroy(scene);
	}

	void SceneManager::SetActiveScene(Scene& scene)
	{
		PT_CORE_ASSERT(Owns(&scene), "Scene '{}' does not belong to this manager", scene.GetName());

		if (m_Updating)
		{
			m_PendingActiveScene = &scene;
			m_ActiveScenePending = true;

			return;
		}

		ApplyActiveScene(&scene);
	}

	void SceneManager::ClearActiveScene()
	{
		if (m_Updating)
		{
			m_PendingActiveScene = nullptr;
			m_ActiveScenePending = true;

			return;
		}

		ApplyActiveScene(nullptr);
	}

	void SceneManager::FixedUpdate(Timestep fixedStep)
	{
		if (m_ActiveScene == nullptr)
		{
			return;
		}

		m_Updating = true;
		m_ActiveScene->FixedUpdate(fixedStep);
		m_Updating = false;
	}

	void SceneManager::Update(Timestep deltaTime)
	{
		if (m_ActiveScene == nullptr)
		{
			return;
		}

		m_Updating = true;
		m_ActiveScene->Update(deltaTime);
		m_Updating = false;
	}

	void SceneManager::SetInterpolationAlpha(float alpha)
	{
		if (m_ActiveScene != nullptr)
		{
			m_ActiveScene->SetInterpolationAlpha(alpha);
		}
	}

	void SceneManager::FlushDeferredChanges()
	{
		// Every scene, not just the active one: a layer may destroy entities in a scene it is preparing
		for (const std::unique_ptr<Scene>& l_Scene : m_Scenes)
		{
			l_Scene->FlushDeferredChanges();
		}

		if (m_ActiveScenePending)
		{
			ApplyActiveScene(m_PendingActiveScene);
			m_PendingActiveScene = nullptr;
			m_ActiveScenePending = false;
		}

		if (!m_PendingDestroy.empty())
		{
			std::vector<Scene*> l_Pending = std::move(m_PendingDestroy);
			m_PendingDestroy.clear();

			for (Scene* l_Scene : l_Pending)
			{
				// The same scene may have been queued twice, so only the pointer is compared
				if (Owns(l_Scene))
				{
					ApplyDestroy(*l_Scene);
				}
			}
		}
	}

	void SceneManager::ApplyActiveScene(Scene* scene)
	{
		if (m_ActiveScene == scene)
		{
			return;
		}

		m_ActiveScene = scene;

		if (scene != nullptr)
		{
			PT_CORE_INFO("Active scene '{}'", scene->GetName());
		}
	}

	void SceneManager::ApplyDestroy(Scene& scene)
	{
		if (m_ActiveScene == &scene)
		{
			m_ActiveScene = nullptr;
		}

		if (m_PendingActiveScene == &scene)
		{
			m_PendingActiveScene = nullptr;
		}

		const auto l_Found = std::find_if(m_Scenes.begin(), m_Scenes.end(), [&scene](const std::unique_ptr<Scene>& owned)
		{
			return owned.get() == &scene;
		});

		if (l_Found != m_Scenes.end())
		{
			m_Scenes.erase(l_Found);
		}
	}

	bool SceneManager::Owns(const Scene* scene) const
	{
		return std::any_of(m_Scenes.begin(), m_Scenes.end(), [scene](const std::unique_ptr<Scene>& owned)
		{
			return owned.get() == scene;
		});
	}
}
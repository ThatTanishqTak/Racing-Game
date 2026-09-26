#pragma once

#include "Powertrain/Core/Log.hpp"
#include "Powertrain/Core/Timestep.hpp"
#include "Powertrain/ECS/Registry.hpp"
#include "Powertrain/Scene/Components.hpp"
#include "Powertrain/Scene/System.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace Powertrain
{
	class EngineContext;

	// One registry, systems grouped into ordered stages and a deferred destroy list. The engine systems are added in the constructor (TransformSystem in PreRender); the game adds its own through AddSystem
	class Scene
	{
	public:
		Scene(std::string name, EngineContext& context);
		~Scene();

		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;

		// Every entity starts with a NameComponent, a TransformComponent and a root HierarchyComponent
		Entity CreateEntity(std::string_view name = "Entity");

		// Applied in FlushDeferredChanges at the end of the frame, never while systems iterate; children go with their parent
		void DestroyEntity(Entity entity);
		bool IsAlive(Entity entity) const { return m_Registry.IsAlive(entity); }

		// Links child under parent, or makes it a root when parent is Entity::Null(). The local transform is left as it is
		void SetParent(Entity child, Entity parent);
		Entity GetParent(Entity entity) const;

		template<typename T, typename... Args>
		T& AddSystem(SystemStage stage, Args&&... args)
		{
			static_assert(std::is_base_of_v<System, T>, "Systems derive from Powertrain::System");
			PT_ASSERT(stage < SystemStage::Count, "Invalid system stage");

			std::unique_ptr<T> l_System = std::make_unique<T>(std::forward<Args>(args)...);
			T& l_Reference = *l_System;

			m_Systems[static_cast<size_t>(stage)].push_back(std::move(l_System));
			l_Reference.OnAttach(*this);

			return l_Reference;
		}

		void SetPrimaryCamera(Entity camera) { m_PrimaryCamera = camera; }
		Entity GetPrimaryCamera() const { return m_PrimaryCamera; }

		const std::string& GetName() const { return m_Name; }
		Registry& GetRegistry() { return m_Registry; }
		const Registry& GetRegistry() const { return m_Registry; }
		EngineContext& GetContext() const { return m_Context; }

		// Fraction of a fixed step elapsed since the last tick, set by the loop before the variable update
		float GetInterpolationAlpha() const { return m_InterpolationAlpha; }

	private:
		friend class SceneManager;

		void FixedUpdate(Timestep fixedStep);
		void Update(Timestep deltaTime);
		void SetInterpolationAlpha(float alpha) { m_InterpolationAlpha = alpha; }
		void FlushDeferredChanges();

		void RunStage(SystemStage stage, Timestep step);
		void DetachFromParent(Entity child);
		void DestroySubtree(Entity entity);

	private:
		std::string m_Name;
		EngineContext& m_Context;
		Registry m_Registry;
		std::array<std::vector<std::unique_ptr<System>>, static_cast<size_t>(SystemStage::Count)> m_Systems;
		std::vector<Entity> m_PendingDestroy;
		Entity m_PrimaryCamera;
		float m_InterpolationAlpha = 0.0f;
	};
}
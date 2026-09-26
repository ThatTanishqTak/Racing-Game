#include "Powertrain/Scene/Scene.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Scene/TransformSystem.hpp"

#include <algorithm>

namespace Powertrain
{
	Scene::Scene(std::string name, EngineContext& context) : m_Name(std::move(name)), m_Context(context)
	{
		// Engine systems first, so game systems added to the same stage run after them
		AddSystem<TransformSystem>(SystemStage::PreRender);

		PT_CORE_INFO("Scene '{}' created", m_Name);
	}

	Scene::~Scene()
	{
		for (auto l_Stage = m_Systems.rbegin(); l_Stage != m_Systems.rend(); ++l_Stage)
		{
			for (auto l_System = l_Stage->rbegin(); l_System != l_Stage->rend(); ++l_System)
			{
				(*l_System)->OnDetach(*this);
			}
		}

		PT_CORE_INFO("Scene '{}' destroyed with {} entities", m_Name, m_Registry.GetAliveCount());
	}

	Entity Scene::CreateEntity(std::string_view name)
	{
		const Entity l_Entity = m_Registry.Create();
		m_Registry.Add<NameComponent>(l_Entity, std::string(name));
		m_Registry.Add<TransformComponent>(l_Entity);
		m_Registry.Add<HierarchyComponent>(l_Entity);

		return l_Entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if (!m_Registry.IsAlive(entity))
		{
			return;
		}

		m_PendingDestroy.push_back(entity);
	}

	void Scene::SetParent(Entity child, Entity parent)
	{
		PT_CORE_ASSERT(IsAlive(child), "SetParent on dead child {}", child.Index);
		PT_CORE_ASSERT(!parent.IsValid() || IsAlive(parent), "SetParent on dead parent {}", parent.Index);
		PT_CORE_ASSERT(child != parent, "Entity {} cannot be its own parent", child.Index);

		// Entities made through CreateEntity always carry the component; ones made through the registry might not. Both adds happen before any reference is taken, because Add can reallocate the pool
		if (!m_Registry.Has<HierarchyComponent>(child))
		{
			m_Registry.Add<HierarchyComponent>(child);
		}
		if (parent.IsValid() && !m_Registry.Has<HierarchyComponent>(parent))
		{
			m_Registry.Add<HierarchyComponent>(parent);
		}

		// Refuse cycles: the new parent must not sit below the child
		for (Entity l_Ancestor = parent; l_Ancestor.IsValid(); l_Ancestor = m_Registry.Get<HierarchyComponent>(l_Ancestor).Parent)
		{
			if (l_Ancestor == child)
			{
				PT_CORE_ASSERT(false, "SetParent would create a cycle through entity {}", child.Index);

				return;
			}
		}

		DetachFromParent(child);

		HierarchyComponent& l_Child = m_Registry.Get<HierarchyComponent>(child);
		l_Child.Parent = parent;

		if (parent.IsValid())
		{
			HierarchyComponent& l_Parent = m_Registry.Get<HierarchyComponent>(parent);
			l_Child.NextSibling = l_Parent.FirstChild;
			l_Parent.FirstChild = child;
		}
	}

	Entity Scene::GetParent(Entity entity) const
	{
		const HierarchyComponent* l_Hierarchy = m_Registry.TryGet<HierarchyComponent>(entity);

		return l_Hierarchy != nullptr ? l_Hierarchy->Parent : Entity::Null();
	}

	void Scene::FixedUpdate(Timestep fixedStep)
	{
		// What the last tick produced becomes the interpolation start for this one
		for (TransformComponent& l_Transform : m_Registry.GetPool<TransformComponent>().GetComponents())
		{
			l_Transform.SnapPrevious();
		}

		RunStage(SystemStage::PrePhysics, fixedStep);
		RunStage(SystemStage::Physics, fixedStep);
		RunStage(SystemStage::PostPhysics, fixedStep);
	}

	void Scene::Update(Timestep deltaTime)
	{
		RunStage(SystemStage::Update, deltaTime);
		RunStage(SystemStage::PreRender, deltaTime);
	}

	void Scene::FlushDeferredChanges()
	{
		m_Registry.FlushDeferred();

		if (m_PendingDestroy.empty())
		{
			return;
		}

		std::vector<Entity> l_Pending = std::move(m_PendingDestroy);
		m_PendingDestroy.clear();

		for (const Entity l_Entity : l_Pending)
		{
			// A child queued after its parent is already gone by now
			if (!m_Registry.IsAlive(l_Entity))
			{
				continue;
			}

			DetachFromParent(l_Entity);
			DestroySubtree(l_Entity);
		}
	}

	void Scene::RunStage(SystemStage stage, Timestep step)
	{
		// Indexed so a system may add another to this stage mid-run; the new one runs at the end of this pass
		std::vector<std::unique_ptr<System>>& l_Systems = m_Systems[static_cast<size_t>(stage)];
		for (size_t l_Index = 0; l_Index < l_Systems.size(); ++l_Index)
		{
			if (IsFixedStage(stage))
			{
				l_Systems[l_Index]->OnFixedUpdate(*this, step);
			}
			else
			{
				l_Systems[l_Index]->OnUpdate(*this, step);
			}
		}
	}

	void Scene::DetachFromParent(Entity child)
	{
		HierarchyComponent& l_Child = m_Registry.Get<HierarchyComponent>(child);
		if (!l_Child.Parent.IsValid())
		{
			l_Child.NextSibling = Entity::Null();

			return;
		}

		HierarchyComponent& l_Parent = m_Registry.Get<HierarchyComponent>(l_Child.Parent);
		if (l_Parent.FirstChild == child)
		{
			l_Parent.FirstChild = l_Child.NextSibling;
		}
		else
		{
			// Singly linked, so walk the siblings to the one before the child
			for (Entity l_Sibling = l_Parent.FirstChild; l_Sibling.IsValid();)
			{
				HierarchyComponent& l_SiblingHierarchy = m_Registry.Get<HierarchyComponent>(l_Sibling);
				if (l_SiblingHierarchy.NextSibling == child)
				{
					l_SiblingHierarchy.NextSibling = l_Child.NextSibling;

					break;
				}

				l_Sibling = l_SiblingHierarchy.NextSibling;
			}
		}

		l_Child.Parent = Entity::Null();
		l_Child.NextSibling = Entity::Null();
	}

	void Scene::DestroySubtree(Entity entity)
	{
		// Children first. Every link is re-read from the pool because Destroy swaps components around
		if (const HierarchyComponent* l_Hierarchy = m_Registry.TryGet<HierarchyComponent>(entity))
		{
			Entity l_Child = l_Hierarchy->FirstChild;
			while (l_Child.IsValid())
			{
				const Entity l_Next = m_Registry.Get<HierarchyComponent>(l_Child).NextSibling;
				DestroySubtree(l_Child);
				l_Child = l_Next;
			}
		}

		if (m_PrimaryCamera == entity)
		{
			m_PrimaryCamera = Entity::Null();
		}

		m_Registry.Destroy(entity);
	}
}
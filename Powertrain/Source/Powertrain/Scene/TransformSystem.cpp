#include "Powertrain/Scene/TransformSystem.hpp"

#include "Powertrain/Scene/Scene.hpp"

#include <span>

namespace Powertrain
{
	void TransformSystem::OnUpdate(Scene& scene, Timestep)
	{
		Registry& l_Registry = scene.GetRegistry();
		Pools l_Pools = { l_Registry.GetPool<TransformComponent>(), l_Registry.GetPool<HierarchyComponent>(), l_Registry.GetPool<WorldTransformComponent>() };
		const float l_Alpha = scene.GetInterpolationAlpha();

		// Start at the roots; children follow through the sibling links so every parent is written before its children.
		// Only the world pool grows here, so the transform entity span stays valid
		const std::span<const Entity> l_Entities = l_Pools.Transforms.GetEntities();
		for (size_t l_Index = 0; l_Index < l_Entities.size(); ++l_Index)
		{
			const Entity l_Entity = l_Entities[l_Index];
			const HierarchyComponent* l_Hierarchy = l_Pools.Hierarchies.TryGet(l_Entity);
			if (l_Hierarchy == nullptr || !l_Hierarchy->Parent.IsValid())
			{
				UpdateSubtree(l_Pools, l_Entity, Matrix4::Identity(), l_Alpha);
			}
		}
	}

	void TransformSystem::UpdateSubtree(Pools& pools, Entity entity, const Matrix4& parentWorld, float alpha)
	{
		Matrix4 l_World = parentWorld;

		if (const TransformComponent* l_Transform = pools.Transforms.TryGet(entity))
		{
			const Vector3 l_Position = Vector3::Lerp(l_Transform->PreviousPosition, l_Transform->Position, alpha);
			const Quaternion l_Rotation = Quaternion::Slerp(l_Transform->PreviousRotation, l_Transform->Rotation, alpha);

			// Local first, then the parent chain
			l_World = Matrix4::TRS(l_Position, l_Rotation, l_Transform->Scale) * parentWorld;

			WorldTransformComponent* l_WorldComponent = pools.Worlds.TryGet(entity);
			if (l_WorldComponent == nullptr)
			{
				l_WorldComponent = &pools.Worlds.Emplace(entity);
			}

			l_WorldComponent->World = l_World;
		}

		if (const HierarchyComponent* l_Hierarchy = pools.Hierarchies.TryGet(entity))
		{
			// Re-read each sibling link from the pool rather than holding a pointer across the recursion
			Entity l_Child = l_Hierarchy->FirstChild;
			while (l_Child.IsValid())
			{
				const Entity l_Next = pools.Hierarchies.Get(l_Child).NextSibling;
				UpdateSubtree(pools, l_Child, l_World, alpha);
				l_Child = l_Next;
			}
		}
	}
}
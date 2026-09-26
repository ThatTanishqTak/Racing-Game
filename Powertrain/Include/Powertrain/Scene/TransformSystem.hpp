#pragma once

#include "Powertrain/ECS/ComponentPool.hpp"
#include "Powertrain/ECS/Entity.hpp"
#include "Powertrain/Math/Matrix4.hpp"
#include "Powertrain/Scene/Components.hpp"
#include "Powertrain/Scene/System.hpp"

namespace Powertrain
{
	// Engine system in PreRender: walks the hierarchy from the roots, blends Previous and Current with the scene's interpolation alpha and writes WorldTransformComponent (adding it where missing). Children multiply onto their parent's world matrix
	class TransformSystem final : public System
	{
	public:
		void OnUpdate(Scene& scene, Timestep deltaTime) override;

	private:
		struct Pools
		{
			ComponentPool<TransformComponent>& Transforms;
			ComponentPool<HierarchyComponent>& Hierarchies;
			ComponentPool<WorldTransformComponent>& Worlds;
		};

		void UpdateSubtree(Pools& pools, Entity entity, const Matrix4& parentWorld, float alpha);
	};
}
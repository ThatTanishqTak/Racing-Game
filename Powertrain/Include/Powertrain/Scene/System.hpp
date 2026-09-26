#pragma once

#include "Powertrain/Core/Timestep.hpp"

#include <cstdint>

namespace Powertrain
{
	class Scene;

	// Fixed ticks run PrePhysics, Physics and PostPhysics through OnFixedUpdate; frames run Update and PreRender through OnUpdate Systems inside a stage run in the order they were added
	enum class SystemStage : uint8_t
	{
		PrePhysics,
		Physics,
		PostPhysics,
		Update,
		PreRender,
		Count
	};

	constexpr bool IsFixedStage(SystemStage stage) { return stage <= SystemStage::PostPhysics; }

	class System
	{
	public:
		virtual ~System() = default;

		virtual void OnAttach(Scene&) {}
		virtual void OnDetach(Scene&) {}
		virtual void OnFixedUpdate(Scene&, Timestep) {}
		virtual void OnUpdate(Scene&, Timestep) {}
	};
}
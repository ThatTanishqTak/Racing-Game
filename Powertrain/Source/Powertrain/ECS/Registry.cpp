#include "Powertrain/ECS/Registry.hpp"

#include "Powertrain/Core/CoreLog.hpp"

namespace Powertrain
{
	Entity Registry::Create()
	{
		++m_AliveCount;

		if (!m_FreeIndices.empty())
		{
			const uint32_t l_Index = m_FreeIndices.back();
			m_FreeIndices.pop_back();

			return { l_Index, m_Generations[l_Index] };
		}

		const uint32_t l_Index = static_cast<uint32_t>(m_Generations.size());
		m_Generations.push_back(0);

		return { l_Index, 0 };
	}

	void Registry::Destroy(Entity entity)
	{
		if (!IsAlive(entity))
		{
			PT_CORE_WARN("Destroy on dead entity {} (generation {})", entity.Index, entity.Generation);

			return;
		}

		if (m_IterationDepth > 0)
		{
			m_Deferred.push_back([entity](Registry& registry)
			{
				registry.Destroy(entity);
			});

			return;
		}

		for (auto& [l_Type, l_Pool] : m_Pools)
		{
			if (l_Pool->Contains(entity))
			{
				l_Pool->Remove(entity);
			}
		}

		// Stale handles now fail the generation check
		++m_Generations[entity.Index];
		m_FreeIndices.push_back(entity.Index);
		--m_AliveCount;
	}

	bool Registry::IsAlive(Entity entity) const
	{
		return entity.IsValid() && entity.Index < m_Generations.size() && m_Generations[entity.Index] == entity.Generation;
	}

	void Registry::FlushDeferred()
	{
		PT_CORE_ASSERT(m_IterationDepth == 0, "FlushDeferred called while Each is iterating");

		// The operations run at depth zero, so none of them re-queues
		std::vector<std::function<void(Registry&)>> l_Deferred = std::move(m_Deferred);
		m_Deferred.clear();

		for (const std::function<void(Registry&)>& l_Operation : l_Deferred)
		{
			l_Operation(*this);
		}
	}
}
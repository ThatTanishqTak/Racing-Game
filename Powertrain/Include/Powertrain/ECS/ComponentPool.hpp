#pragma once

#include "Powertrain/Core/Log.hpp"
#include "Powertrain/ECS/Entity.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace Powertrain
{
	// Type-erased face of a pool, so the Registry can strip a destroyed entity from every pool and lock the ones Each walks
	class ComponentPoolBase
	{
	public:
		virtual ~ComponentPoolBase() = default;

		virtual bool Contains(Entity entity) const = 0;
		virtual void Remove(Entity entity) = 0;
		virtual size_t Size() const = 0;
		virtual std::span<const Entity> GetEntities() const = 0;

		// Each locks the pools it iterates: a locked pool asserts on Emplace and the Registry defers Remove and Destroy,
		// so the references handed to the callback stay valid
		void Lock() { ++m_LockCount; }
		void Unlock() { PT_ASSERT(m_LockCount > 0, "Component pool unlocked more often than locked"); --m_LockCount; }
		bool IsLocked() const { return m_LockCount > 0; }

	private:
		uint32_t m_LockCount = 0;
	};

	// Sparse set. m_Sparse maps an entity index to a dense index (k_Absent when missing); m_Entities and m_Components stay
	// packed through swap-and-pop removal, so iteration is a linear walk. Switch m_Sparse to pages if entity counts grow.
	template<typename T>
	class ComponentPool final : public ComponentPoolBase
	{
	public:
		static constexpr uint32_t k_Absent = UINT32_MAX;

		// Adding may reallocate and dangle earlier references into this pool
		template<typename... Args>
		T& Emplace(Entity entity, Args&&... args)
		{
			PT_ASSERT(!IsLocked(), "Cannot add a component to a pool while Each iterates it");
			PT_ASSERT(!Contains(entity), "Entity {} already has this component", entity.Index);

			if (entity.Index >= m_Sparse.size())
			{
				m_Sparse.resize(static_cast<size_t>(entity.Index) + 1, k_Absent);
			}

			m_Sparse[entity.Index] = static_cast<uint32_t>(m_Components.size());
			m_Entities.push_back(entity);

			return m_Components.emplace_back(std::forward<Args>(args)...);
		}

		bool Contains(Entity entity) const override
		{
			if (!entity.IsValid() || entity.Index >= m_Sparse.size())
			{
				return false;
			}

			const uint32_t l_Dense = m_Sparse[entity.Index];

			return l_Dense != k_Absent && m_Entities[l_Dense].Generation == entity.Generation;
		}

		void Remove(Entity entity) override
		{
			PT_ASSERT(!IsLocked(), "Cannot remove a component from a pool while Each iterates it");
			PT_ASSERT(Contains(entity), "Entity {} does not have this component", entity.Index);

			const uint32_t l_Dense = m_Sparse[entity.Index];
			const uint32_t l_Last = static_cast<uint32_t>(m_Components.size() - 1);

			// Move the last element into the hole so the dense arrays stay packed
			if (l_Dense != l_Last)
			{
				m_Components[l_Dense] = std::move(m_Components[l_Last]);
				m_Entities[l_Dense] = m_Entities[l_Last];
				m_Sparse[m_Entities[l_Dense].Index] = l_Dense;
			}

			m_Components.pop_back();
			m_Entities.pop_back();
			m_Sparse[entity.Index] = k_Absent;
		}

		size_t Size() const override { return m_Components.size(); }
		std::span<const Entity> GetEntities() const override { return m_Entities; }

		std::span<T> GetComponents() { return m_Components; }
		std::span<const T> GetComponents() const { return m_Components; }

		T& Get(Entity entity)
		{
			PT_ASSERT(Contains(entity), "Entity {} does not have this component", entity.Index);

			return m_Components[m_Sparse[entity.Index]];
		}

		const T& Get(Entity entity) const
		{
			PT_ASSERT(Contains(entity), "Entity {} does not have this component", entity.Index);

			return m_Components[m_Sparse[entity.Index]];
		}

		T* TryGet(Entity entity) { return Contains(entity) ? &m_Components[m_Sparse[entity.Index]] : nullptr; }
		const T* TryGet(Entity entity) const { return Contains(entity) ? &m_Components[m_Sparse[entity.Index]] : nullptr; }

	private:
		std::vector<uint32_t> m_Sparse;
		std::vector<Entity> m_Entities;
		std::vector<T> m_Components;
	};
}
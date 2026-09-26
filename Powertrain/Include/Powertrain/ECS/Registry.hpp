#pragma once

#include "Powertrain/Core/Log.hpp"
#include "Powertrain/ECS/ComponentPool.hpp"
#include "Powertrain/ECS/Entity.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Powertrain
{
	// Generational entity ids plus one sparse-set pool per component type. Pools are keyed by std::type_index and owned by this registry, so there is no static counter and two registries never share state.
	class Registry
	{
	public:
		Registry() = default;

		Registry(const Registry&) = delete;
		Registry& operator=(const Registry&) = delete;

		Entity Create();

		// Strips every component and bumps the generation. Deferred to FlushDeferred while Each iterates
		void Destroy(Entity entity);

		bool IsAlive(Entity entity) const;
		uint32_t GetAliveCount() const { return m_AliveCount; }

		template<typename T, typename... Args>
		T& Add(Entity entity, Args&&... args)
		{
			PT_ASSERT(IsAlive(entity), "Add on dead entity {}", entity.Index);

			return GetPool<T>().Emplace(entity, std::forward<Args>(args)...);
		}

		// No-op when the entity lacks the component. Deferred to FlushDeferred while Each iterates the pool
		template<typename T>
		void Remove(Entity entity)
		{
			PT_ASSERT(IsAlive(entity), "Remove on dead entity {}", entity.Index);

			ComponentPool<T>& l_Pool = GetPool<T>();
			if (!l_Pool.Contains(entity))
			{
				return;
			}

			if (l_Pool.IsLocked())
			{
				m_Deferred.push_back([entity](Registry& registry)
				{
					if (registry.IsAlive(entity))
					{
						registry.Remove<T>(entity);
					}
				});

				return;
			}

			l_Pool.Remove(entity);
		}

		template<typename T>
		bool Has(Entity entity) const
		{
			const ComponentPool<T>* l_Pool = TryGetPool<T>();

			return l_Pool != nullptr && l_Pool->Contains(entity);
		}

		template<typename T>
		T& Get(Entity entity)
		{
			return GetPool<T>().Get(entity);
		}

		template<typename T>
		const T& Get(Entity entity) const
		{
			const ComponentPool<T>* l_Pool = TryGetPool<T>();
			PT_ASSERT(l_Pool != nullptr, "No pool for this component type");

			return l_Pool->Get(entity);
		}

		template<typename T>
		T* TryGet(Entity entity)
		{
			ComponentPool<T>* l_Pool = TryGetPool<T>();

			return l_Pool != nullptr ? l_Pool->TryGet(entity) : nullptr;
		}

		template<typename T>
		const T* TryGet(Entity entity) const
		{
			const ComponentPool<T>* l_Pool = TryGetPool<T>();

			return l_Pool != nullptr ? l_Pool->TryGet(entity) : nullptr;
		}

		// Calls function(Entity, T&...) for every entity that has all of the listed components, walking the smallest pool. The pools are locked meanwhile: adding one of them asserts, removes and destroys apply when the outermost Each returns
		template<typename... T, typename Function>
		void Each(Function&& function)
		{
			static_assert(sizeof...(T) > 0, "Each needs at least one component type");

			std::tuple<ComponentPool<T>&...> l_Pools(GetPool<T>()...);
			EachImplementation(l_Pools, std::forward<Function>(function), std::index_sequence_for<T...>{});
		}

		// Systems cache the pool instead of hashing the type on every Get; the pool is created on first use
		template<typename T>
		ComponentPool<T>& GetPool()
		{
			const std::type_index l_Type(typeid(T));

			auto l_Found = m_Pools.find(l_Type);
			if (l_Found == m_Pools.end())
			{
				l_Found = m_Pools.emplace(l_Type, std::make_unique<ComponentPool<T>>()).first;
			}

			return static_cast<ComponentPool<T>&>(*l_Found->second);
		}

		template<typename T>
		ComponentPool<T>* TryGetPool()
		{
			const auto l_Found = m_Pools.find(std::type_index(typeid(T)));

			return l_Found != m_Pools.end() ? static_cast<ComponentPool<T>*>(l_Found->second.get()) : nullptr;
		}

		template<typename T>
		const ComponentPool<T>* TryGetPool() const
		{
			const auto l_Found = m_Pools.find(std::type_index(typeid(T)));

			return l_Found != m_Pools.end() ? static_cast<const ComponentPool<T>*>(l_Found->second.get()) : nullptr;
		}

		// Applies the destroys and removes queued during iteration; runs automatically when the outermost Each returns
		void FlushDeferred();
		bool IsIterating() const { return m_IterationDepth > 0; }

	private:
		template<typename Pools, typename Function, size_t... I>
		void EachImplementation(Pools& pools, Function&& function, std::index_sequence<I...>)
		{
			const std::array<ComponentPoolBase*, sizeof...(I)> l_Bases = { &std::get<I>(pools)... };

			ComponentPoolBase* l_Smallest = l_Bases[0];
			for (ComponentPoolBase* l_Base : l_Bases)
			{
				if (l_Base->Size() < l_Smallest->Size())
				{
					l_Smallest = l_Base;
				}
			}

			for (ComponentPoolBase* l_Base : l_Bases)
			{
				l_Base->Lock();
			}
			++m_IterationDepth;

			// Locked pools cannot grow or shrink, so the span stays valid for the whole walk
			const std::span<const Entity> l_Entities = l_Smallest->GetEntities();
			for (size_t l_Index = 0; l_Index < l_Entities.size(); ++l_Index)
			{
				const Entity l_Entity = l_Entities[l_Index];
				if ((std::get<I>(pools).Contains(l_Entity) && ...))
				{
					function(l_Entity, std::get<I>(pools).Get(l_Entity)...);
				}
			}

			--m_IterationDepth;
			for (ComponentPoolBase* l_Base : l_Bases)
			{
				l_Base->Unlock();
			}

			if (m_IterationDepth == 0)
			{
				FlushDeferred();
			}
		}

	private:
		std::vector<uint32_t> m_Generations;
		std::vector<uint32_t> m_FreeIndices;
		std::unordered_map<std::type_index, std::unique_ptr<ComponentPoolBase>> m_Pools;
		std::vector<std::function<void(Registry&)>> m_Deferred;
		uint32_t m_AliveCount = 0;
		uint32_t m_IterationDepth = 0;
	};
}
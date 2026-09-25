#pragma once

#include "Powertrain/Core/Log.hpp"
#include "Powertrain/Core/Timestep.hpp"

#include <string>
#include <utility>

namespace Powertrain
{
	class EngineContext;
	class Event;

	class Layer
	{
	public:
		explicit Layer(std::string name = "Layer") : m_Name(std::move(name)) {}
		virtual ~Layer() = default;

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnEvent(Event&) {}
		virtual void OnFixedUpdate(Timestep) {}
		virtual void OnUpdate(Timestep) {}
		virtual void OnRender() {}

		const std::string& GetName() const { return m_Name; }

	protected:
		EngineContext& GetContext() const
		{
			PT_ASSERT(m_Context != nullptr, "GetContext() called on layer '{}' before it was attached", m_Name);

			return *m_Context;
		}

	private:
		friend class ApplicationImplementation;

		std::string m_Name;
		EngineContext* m_Context = nullptr;
	};
}
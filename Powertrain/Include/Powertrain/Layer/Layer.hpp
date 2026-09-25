#pragma once

#include "Powertrain/Core/Timestep.hpp"

#include <string>

namespace Powertrain
{
	class EngineContext;
	class Event;

	class Layer
	{
	public:
		Layer(const std::string& name = "Layer") : m_Name(name) {}
		virtual ~Layer() = default;

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnEvent(Event&) {}
		virtual void OnFixedUpdate(Timestep) {}
		virtual void OnUpdate(Timestep) {}

		const std::string& GetName() const { return m_Name; }

	protected:
		EngineContext& GetContext() const { return *m_Context; }

	protected:
		std::string m_Name;

	private:
		friend class ApplicationImplementation;

		EngineContext* m_Context = nullptr;
	};
}
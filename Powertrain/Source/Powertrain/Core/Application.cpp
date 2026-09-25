#include "Powertrain/Core/Application.hpp"

#include "Powertrain/Core/ApplicationImplementation.hpp"

#include <utility>

namespace Powertrain
{
	Application::Application(const ApplicationSpecification& specification) : m_Implementation(std::make_unique<ApplicationImplementation>(specification))
	{

	}

	Application::~Application() = default;

	void Application::PushLayer(std::unique_ptr<Layer> layer)
	{
		m_Implementation->PushLayer(std::move(layer));
	}

	void Application::PushOverlay(std::unique_ptr<Layer> overlay)
	{
		m_Implementation->PushOverlay(std::move(overlay));
	}

	const ApplicationSpecification& Application::GetSpecification() const
	{
		return m_Implementation->GetSpecification();
	}
}
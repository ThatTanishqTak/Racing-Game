#include "Powertrain/Powertrain.hpp"

#include "Sandbox/SandboxLayer.hpp"

class SandboxApplication : public Powertrain::Application
{
public:
	SandboxApplication(const Powertrain::ApplicationSpecification& specification) : Powertrain::Application(specification)
	{
		PushLayer(std::make_unique<SandboxLayer>());
	}
};

std::unique_ptr<Powertrain::Application> Powertrain::CreateApplication()
{
	Powertrain::ApplicationSpecification l_Specification;
	l_Specification.Window.Title = "Sandbox";

	return std::make_unique<SandboxApplication>(l_Specification);
}
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
	l_Specification.Name = "Sandbox";
	l_Specification.Window.Title = "Sandbox";
	l_Specification.Renderer.ClearColor = { 0.05f, 0.20f, 0.40f, 1.0f };

	return std::make_unique<SandboxApplication>(l_Specification);
}
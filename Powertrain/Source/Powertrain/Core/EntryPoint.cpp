#include "Powertrain/Core/Application.hpp"
#include "Powertrain/Core/ApplicationHost.hpp"

#include <memory>

int main()
{
	std::unique_ptr<Powertrain::Application> l_Application = Powertrain::CreateApplication();
	Powertrain::ApplicationHost l_Host;

	return l_Host.Run(*l_Application);
}
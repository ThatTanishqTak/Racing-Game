#include "Powertrain/Core/ApplicationHost.hpp"

#include "Powertrain/Core/Application.hpp"
#include "Powertrain/Core/ApplicationImplementation.hpp"

namespace Powertrain
{
	int ApplicationHost::Run(Application& application)
	{
		ApplicationImplementation& l_Implementation = *application.m_Implementation;

		if (!l_Implementation.Initialize())
		{
			l_Implementation.Shutdown();

			return 1;
		}

		l_Implementation.Run();
		l_Implementation.Shutdown();

		return 0;
	}
}
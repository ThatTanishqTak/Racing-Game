#pragma once

namespace Powertrain
{
	class Application;

	class ApplicationHost
	{
	public:
		int Run(Application& application);
	};
}
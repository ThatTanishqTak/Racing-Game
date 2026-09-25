#pragma once

#include "Powertrain/Layer/Layer.hpp"
#include "Powertrain/Window/Window.hpp"

#include <cstdint>
#include <memory>

namespace Powertrain
{
	struct ApplicationSpecification
	{
		WindowProperties Window;

		double FixedUpdateRate = 240.0;
		uint32_t MaxFixedStepsPerFrame = 8;
	};

	class ApplicationImplementation;

	class Application
	{
	public:
		explicit Application(const ApplicationSpecification& specification = ApplicationSpecification());
		virtual ~Application();

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		void PushLayer(std::unique_ptr<Layer> layer);
		void PushOverlay(std::unique_ptr<Layer> overlay);

		const ApplicationSpecification& GetSpecification() const;

	private:
		friend class ApplicationHost;

		std::unique_ptr<ApplicationImplementation> m_Implementation;
	};

	std::unique_ptr<Application> CreateApplication();
}
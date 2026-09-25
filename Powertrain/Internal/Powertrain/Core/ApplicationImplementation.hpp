#pragma once

#include "Powertrain/Core/Application.hpp"
#include "Powertrain/Core/EngineContext.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace Powertrain
{
	class Event;
	class WindowsWindow;

	class ApplicationImplementation
	{
	public:
		explicit ApplicationImplementation(const ApplicationSpecification& specification);
		~ApplicationImplementation();

		bool Initialize();
		void Run();
		void Shutdown();

		void PushLayer(std::unique_ptr<Layer> layer);
		void PushOverlay(std::unique_ptr<Layer> overlay);

		const ApplicationSpecification& GetSpecification() const { return m_Specification; }

	private:
		struct PendingLayer
		{
			std::unique_ptr<Layer> Instance;
			bool Overlay = false;
		};

		void InsertLayer(std::unique_ptr<Layer> layer);
		void InsertOverlay(std::unique_ptr<Layer> overlay);
		void FlushPendingLayers();
		void AttachLayer(Layer& layer);
		void OnEvent(Event& event);

	private:
		ApplicationSpecification m_Specification;

		std::unique_ptr<WindowsWindow> m_Window;
		std::unique_ptr<EngineContext> m_Context;

		std::vector<std::unique_ptr<Layer>> m_LayerStack;
		std::vector<PendingLayer> m_PendingLayers;
		size_t m_LayerInsertIndex = 0;

		bool m_Initialized = false;
		bool m_InFrame = false;
	};
}
#include "Powertrain/Core/ApplicationImplementation.hpp"

#include "Powertrain/Core/Clock.hpp"
#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Events/Event.hpp"
#include "Powertrain/Platform/Windows/WindowsWindow.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace Powertrain
{
	ApplicationImplementation::ApplicationImplementation(const ApplicationSpecification& specification) : m_Specification(specification)
	{

	}

	ApplicationImplementation::~ApplicationImplementation() = default;

	bool ApplicationImplementation::Initialize()
	{
		PT_CORE_INFO("Initializing application '{}'", m_Specification.Window.Title);
		PT_CORE_ASSERT(m_Specification.FixedUpdateRate > 0.0, "FixedUpdateRate must be positive");
		PT_CORE_ASSERT(m_Specification.MaxFixedStepsPerFrame > 0, "MaxFixedStepsPerFrame must be positive");

		m_Window = std::make_unique<WindowsWindow>();
		if (!m_Window->OnInitialize(m_Specification.Window))
		{
			PT_CORE_ERROR("Window initialization failed");

			return false;
		}

		m_Context = std::make_unique<EngineContext>(*m_Window);
		m_Initialized = true;

		for (const std::unique_ptr<Layer>& l_Layer : m_LayerStack)
		{
			AttachLayer(*l_Layer);
		}

		return true;
	}

	void ApplicationImplementation::Run()
	{
		const double l_FixedStep = 1.0 / m_Specification.FixedUpdateRate;
		double l_Accumulator = 0.0;

		Clock l_Clock;
		m_InFrame = true;

		while (!m_Context->IsExitRequested() && !m_Window->ShouldClose())
		{
			const double l_FrameTime = std::min(l_Clock.Restart(), 0.25);

			m_Window->OnUpdate();

			l_Accumulator += l_FrameTime;

			uint32_t l_StepCount = 0;
			while (l_Accumulator >= l_FixedStep && l_StepCount < m_Specification.MaxFixedStepsPerFrame)
			{
				for (const std::unique_ptr<Layer>& l_Layer : m_LayerStack)
				{
					l_Layer->OnFixedUpdate(Timestep(l_FixedStep));
				}

				l_Accumulator -= l_FixedStep;
				++l_StepCount;
			}

			if (l_Accumulator >= l_FixedStep)
			{
				PT_CORE_WARN("Simulation behind, dropped {:.2f} ms", l_Accumulator * 1000.0);
				l_Accumulator = 0.0;
			}

			const Timestep l_DeltaTime(l_FrameTime);
			for (const std::unique_ptr<Layer>& l_Layer : m_LayerStack)
			{
				l_Layer->OnUpdate(l_DeltaTime);
			}

			FlushPendingLayers();
		}

		m_InFrame = false;
	}

	void ApplicationImplementation::Shutdown()
	{
		m_PendingLayers.clear();

		if (m_Initialized)
		{
			for (auto l_Iterator = m_LayerStack.rbegin(); l_Iterator != m_LayerStack.rend(); ++l_Iterator)
			{
				(*l_Iterator)->OnDetach();
				(*l_Iterator)->m_Context = nullptr;
			}
		}

		m_LayerStack.clear();
		m_LayerInsertIndex = 0;
		m_Context.reset();

		if (m_Window)
		{
			m_Window->OnShutdown();
			m_Window.reset();
		}

		m_Initialized = false;

		PT_CORE_INFO("Application shut down");
	}

	void ApplicationImplementation::PushLayer(std::unique_ptr<Layer> layer)
	{
		if (m_InFrame)
		{
			m_PendingLayers.push_back({ std::move(layer), false });

			return;
		}

		InsertLayer(std::move(layer));
	}

	void ApplicationImplementation::PushOverlay(std::unique_ptr<Layer> overlay)
	{
		if (m_InFrame)
		{
			m_PendingLayers.push_back({ std::move(overlay), true });

			return;
		}

		InsertOverlay(std::move(overlay));
	}

	void ApplicationImplementation::InsertLayer(std::unique_ptr<Layer> layer)
	{
		Layer& l_Layer = *layer;

		m_LayerStack.insert(m_LayerStack.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex), std::move(layer));
		++m_LayerInsertIndex;

		AttachLayer(l_Layer);
	}

	void ApplicationImplementation::InsertOverlay(std::unique_ptr<Layer> overlay)
	{
		Layer& l_Overlay = *overlay;

		m_LayerStack.push_back(std::move(overlay));

		AttachLayer(l_Overlay);
	}

	void ApplicationImplementation::FlushPendingLayers()
	{
		std::vector<PendingLayer> l_Pending = std::move(m_PendingLayers);
		m_PendingLayers.clear();

		for (PendingLayer& l_Entry : l_Pending)
		{
			if (l_Entry.Overlay)
			{
				InsertOverlay(std::move(l_Entry.Instance));
			}
			else
			{
				InsertLayer(std::move(l_Entry.Instance));
			}
		}
	}

	void ApplicationImplementation::AttachLayer(Layer& layer)
	{
		if (!m_Initialized)
		{
			return;
		}

		layer.m_Context = m_Context.get();
		layer.OnAttach();
	}

	void ApplicationImplementation::OnEvent(Event& event)
	{
		for (auto l_Iterator = m_LayerStack.rbegin(); l_Iterator != m_LayerStack.rend(); ++l_Iterator)
		{
			if (event.IsHandled())
			{
				break;
			}

			(*l_Iterator)->OnEvent(event);
		}

		event.Dispatch<WindowCloseEvent>([this](const WindowCloseEvent&)
		{
			PT_CORE_INFO("Window close requested");
			m_Context->RequestExit();

			return true;
		});
	}
}
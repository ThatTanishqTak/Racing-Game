#include "Powertrain/Core/ApplicationImplementation.hpp"

#include "Powertrain/Core/Clock.hpp"
#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Events/Event.hpp"
#include "Powertrain/Platform/Windows/Input/WindowsInput.hpp"
#include "Powertrain/Platform/Windows/WindowsWindow.hpp"
#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"
#include "Powertrain/Scene/SceneManager.hpp"

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
		PT_CORE_INFO("Initializing application '{}'", m_Specification.Name);
		PT_CORE_ASSERT(m_Specification.FixedUpdateRate > 0.0, "FixedUpdateRate must be positive");
		PT_CORE_ASSERT(m_Specification.MaxFixedStepsPerFrame > 0, "MaxFixedStepsPerFrame must be positive");

		const EventCallback l_Callback = [this](Event& event)
		{
			OnEvent(event);
		};

		m_Window = std::make_unique<WindowsWindow>();
		if (!m_Window->Initialize(m_Specification.Window, l_Callback))
		{
			PT_CORE_ERROR("Window initialization failed");

			return false;
		}

		m_Input = std::make_unique<WindowsInput>();
		if (!m_Input->Initialize(*m_Window, l_Callback))
		{
			PT_CORE_ERROR("Input initialization failed");

			return false;
		}

		m_Renderer = std::make_unique<D3D12Renderer>();
		if (!m_Renderer->Initialize(*m_Window, m_Specification.Renderer))
		{
			PT_CORE_ERROR("Renderer initialization failed");

			return false;
		}

		m_Scenes = std::make_unique<SceneManager>();
		m_Context = std::make_unique<EngineContext>(*m_Window, *m_Input, *m_Renderer, *m_Scenes);
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

		while (!m_Context->IsExitRequested())
		{
			// Block on the swap chain before sampling input, so the frame's input is as fresh as possible
			m_Renderer->WaitForNextFrame();

			const double l_FrameTime = std::min(l_Clock.Restart(), 0.25);

			m_Input->BeginFrame();
			m_Window->PollEvents();
			m_Input->Update(l_FrameTime);

			if (m_Window->IsMinimized() && !m_Context->IsExitRequested())
			{
				// Sleep until the next message instead of spinning
				m_Window->WaitForMessages();
				l_Clock.Restart();

				continue;
			}

			l_Accumulator += l_FrameTime;

			uint32_t l_StepCount = 0;
			m_Input->BeginFixedPhase();
			while (l_Accumulator >= l_FixedStep && l_StepCount < m_Specification.MaxFixedStepsPerFrame)
			{
				for (const std::unique_ptr<Layer>& l_Layer : m_LayerStack)
				{
					l_Layer->OnFixedUpdate(Timestep(l_FixedStep));
				}

				// Scene fixed stages: PrePhysics, Physics, PostPhysics
				m_Scenes->FixedUpdate(Timestep(l_FixedStep));

				m_Input->EndFixedStep();

				l_Accumulator -= l_FixedStep;
				++l_StepCount;
			}
			m_Input->EndFixedPhase();

			if (l_Accumulator >= l_FixedStep)
			{
				PT_CORE_WARN("Simulation behind, dropped {:.2f} ms", l_Accumulator * 1000.0);
				l_Accumulator = 0.0;
			}

			// How far the frame sits between the last tick and the next; TransformSystem blends with it
			m_Scenes->SetInterpolationAlpha(static_cast<float>(l_Accumulator / l_FixedStep));

			const Timestep l_DeltaTime(l_FrameTime);
			for (const std::unique_ptr<Layer>& l_Layer : m_LayerStack)
			{
				l_Layer->OnUpdate(l_DeltaTime);
			}

			// Scene frame stages: Update, then PreRender writes the world transforms the layers draw from
			m_Scenes->Update(l_DeltaTime);

			bool l_Rendered = m_Renderer->BeginFrame();
			if (l_Rendered)
			{
				// The scene goes down first, so the layers' debug lines and the UI land on top of it
				m_Renderer->RenderScene();

				for (const std::unique_ptr<Layer>& l_Layer : m_LayerStack)
				{
					l_Layer->OnRender();
				}

				// Debug lines gathered anywhere this frame go down before the UI
				m_Renderer->RenderDebugDraw();

				// ImGui is the last pass
				m_Renderer->BeginImGuiFrame();
				for (const std::unique_ptr<Layer>& l_Layer : m_LayerStack)
				{
					l_Layer->OnImGuiRender();
				}
				m_Renderer->EndImGuiFrame();

				l_Rendered = m_Renderer->EndFrame();
			}

			if (!l_Rendered)
			{
				PT_CORE_FATAL("Rendering failed, exiting");
				m_Context->RequestExit();
			}

			// Entity destruction and scene switches requested this frame land here, never while systems iterate
			m_Scenes->FlushDeferredChanges();

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

		m_Initialized = false;

		m_LayerStack.clear();
		m_LayerInsertIndex = 0;

		m_Scenes.reset();
		m_Context.reset();

		if (m_Renderer)
		{
			m_Renderer->Shutdown();
			m_Renderer.reset();
		}

		if (m_Window)
		{
			m_Window->Shutdown();
			m_Window.reset();
		}

		if (m_Input)
		{
			m_Input->Shutdown();
			m_Input.reset();
		}

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
		if (!m_Initialized)
		{
			return;
		}

		event.Dispatch<WindowResizeEvent>([this](const WindowResizeEvent& resizeEvent)
		{
			PT_CORE_TRACE("Window resized to {}x{}", resizeEvent.Width, resizeEvent.Height);
			m_Renderer->Resize(resizeEvent.Width, resizeEvent.Height);

			return false;
		});

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
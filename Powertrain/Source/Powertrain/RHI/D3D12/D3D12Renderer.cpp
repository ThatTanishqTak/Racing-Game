#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Platform/Windows/WindowsWindow.hpp"

namespace Powertrain
{
	namespace
	{
		// Heap sizes. The resource heap is 65,536 slots total: persistent plus two frames of transients.
		constexpr uint32_t k_RtvCount = 64;
		constexpr uint32_t k_DsvCount = 16;
		constexpr uint32_t k_PersistentResourceCount = 49152;
		constexpr uint32_t k_TransientResourceCountPerFrame = 8192;
		constexpr uint32_t k_SamplerCount = 2048;

		// Draws nothing until the line batcher arrives
		class NullDebugDraw final : public DebugDraw
		{
		public:
			void Line(const Vector3&, const Vector3&, const Color&) override {}
			void Arrow(const Vector3&, const Vector3&, const Color&) override {}
			void Box(const Matrix4&, const Vector3&, const Color&) override {}
			void Sphere(const Vector3&, float, const Color&) override {}
		};

		D3D12_RESOURCE_BARRIER MakeTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
		{
			D3D12_RESOURCE_BARRIER l_Barrier = {};
			l_Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			l_Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			l_Barrier.Transition.pResource = resource;
			l_Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			l_Barrier.Transition.StateBefore = before;
			l_Barrier.Transition.StateAfter = after;

			return l_Barrier;
		}
	}

	D3D12Renderer::D3D12Renderer() = default;

	D3D12Renderer::~D3D12Renderer()
	{
		Shutdown();
	}

	bool D3D12Renderer::Initialize(WindowsWindow& window, const RendererSettings& settings)
	{
		m_ClearColor = settings.ClearColor;

		DeviceSettings l_DeviceSettings;
		l_DeviceSettings.EnableDebugLayer = settings.EnableDebugLayer;
		l_DeviceSettings.EnableGpuBasedValidation = settings.EnableGpuBasedValidation;

		if (!m_Device.Initialize(l_DeviceSettings))
		{
			return false;
		}

		if (!m_DirectQueue.Initialize(m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT, "Direct Queue") || !m_CopyQueue.Initialize(m_Device, D3D12_COMMAND_LIST_TYPE_COPY, "Copy Queue"))
		{
			return false;
		}

		if (!m_CommandList.Initialize(m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT, "Frame Command List"))
		{
			return false;
		}

		if (!m_GpuTimer.Initialize(m_Device, m_DirectQueue, "GPU Timer"))
		{
			return false;
		}

		if (!m_RtvHeap.Initialize(m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, k_RtvCount, 0, false, "RTV Heap") ||
			!m_DsvHeap.Initialize(m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, k_DsvCount, 0, false, "DSV Heap") ||
			!m_ResourceHeap.Initialize(m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, k_PersistentResourceCount, k_TransientResourceCountPerFrame, true, "Resource Heap") ||
			!m_SamplerHeap.Initialize(m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, k_SamplerCount, 0, true, "Sampler Heap"))
		{
			return false;
		}

		if (!m_SwapChain.Initialize(m_Device, m_DirectQueue, m_RtvHeap, window.GetHandle(), window.GetWidth(), window.GetHeight(), settings.VSync))
		{
			return false;
		}

		if (!m_ImGuiPass.Initialize(*this, window))
		{
			return false;
		}

		m_DebugDraw = std::make_unique<NullDebugDraw>();

		m_FrameFenceValues.fill(0);
		m_FrameIndex = 0;
		m_FrameDrawCalls = 0;
		m_FrameTriangles = 0;
		m_Stats = RendererStats();
		m_LastFrameEnd = std::chrono::steady_clock::now();
		m_ResizePending = false;
		m_InFrame = false;
		m_FrameSlotAcquired = false;
		m_DeviceLost = false;
		m_Initialized = true;

		PT_CORE_INFO("Renderer initialized");

		return true;
	}

	void D3D12Renderer::Shutdown()
	{
		if (m_InFrame)
		{
			m_CommandList.Close();
			m_InFrame = false;
		}

		if (m_DirectQueue.GetHandle() != nullptr)
		{
			m_DirectQueue.Flush();
		}

		if (m_CopyQueue.GetHandle() != nullptr)
		{
			m_CopyQueue.Flush();
		}

		m_DeferredRelease.Flush();

		// Reverse creation order
		m_ImGuiPass.Shutdown();
		m_SwapChain.Shutdown();
		m_SamplerHeap.Shutdown();
		m_ResourceHeap.Shutdown();
		m_DsvHeap.Shutdown();
		m_RtvHeap.Shutdown();
		m_GpuTimer.Shutdown();
		m_CommandList.Shutdown();
		m_CopyQueue.Shutdown();
		m_DirectQueue.Shutdown();
		m_DebugDraw.reset();
		m_Device.Shutdown();

		if (m_Initialized)
		{
			PT_CORE_INFO("Renderer shut down");
		}

		m_Initialized = false;
	}

	void D3D12Renderer::WaitForNextFrame()
	{
		// The latency object is a semaphore that Present releases; never take a second slot before presenting the first
		if (!m_Initialized || m_DeviceLost || m_FrameSlotAcquired)
		{
			return;
		}

		m_SwapChain.WaitForNextFrame();
		m_FrameSlotAcquired = true;
	}

	bool D3D12Renderer::BeginFrame()
	{
		PT_CORE_ASSERT(!m_InFrame, "BeginFrame called twice without EndFrame");

		if (!m_Initialized || m_DeviceLost)
		{
			return false;
		}

		if (m_ResizePending && !ApplyPendingResize())
		{
			m_DeviceLost = true;

			return false;
		}

		// The allocator for this frame index was last used k_FramesInFlight frames ago
		m_DirectQueue.WaitForFence(m_FrameFenceValues[m_FrameIndex]);
		m_DeferredRelease.Release(m_DirectQueue.GetCompletedValue());

		// Same slot, same guarantee
		m_GpuTimer.BeginFrame(m_FrameIndex);

		m_ResourceHeap.BeginFrame(m_FrameIndex);
		m_SamplerHeap.BeginFrame(m_FrameIndex);

		if (!m_CommandList.Reset(m_FrameIndex))
		{
			m_DeviceLost = true;

			return false;
		}

		ID3D12GraphicsCommandList* l_List = m_CommandList.GetHandle();

		ID3D12DescriptorHeap* l_Heaps[] = { m_ResourceHeap.GetHandle(), m_SamplerHeap.GetHandle() };
		l_List->SetDescriptorHeaps(2, l_Heaps);

		m_GpuTimer.Begin(l_List, D3D12GpuTimer::k_FrameTimer);

		const D3D12_RESOURCE_BARRIER l_ToRenderTarget = MakeTransition(m_SwapChain.GetCurrentBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		l_List->ResourceBarrier(1, &l_ToRenderTarget);

		const D3D12_CPU_DESCRIPTOR_HANDLE l_Rtv = m_SwapChain.GetCurrentRtv();
		const float l_Clear[4] = { m_ClearColor.R, m_ClearColor.G, m_ClearColor.B, m_ClearColor.A };
		l_List->OMSetRenderTargets(1, &l_Rtv, FALSE, nullptr);
		l_List->ClearRenderTargetView(l_Rtv, l_Clear, 0, nullptr);

		D3D12_VIEWPORT l_Viewport = {};
		l_Viewport.Width = static_cast<float>(m_SwapChain.GetWidth());
		l_Viewport.Height = static_cast<float>(m_SwapChain.GetHeight());
		l_Viewport.MinDepth = 0.0f;
		l_Viewport.MaxDepth = 1.0f;
		l_List->RSSetViewports(1, &l_Viewport);

		const D3D12_RECT l_Scissor = { 0, 0, static_cast<LONG>(m_SwapChain.GetWidth()), static_cast<LONG>(m_SwapChain.GetHeight()) };
		l_List->RSSetScissorRects(1, &l_Scissor);

		m_FrameDrawCalls = 0;
		m_FrameTriangles = 0;
		m_InFrame = true;

		return true;
	}

	bool D3D12Renderer::EndFrame()
	{
		PT_CORE_ASSERT(m_InFrame, "EndFrame called without BeginFrame");
		m_InFrame = false;

		if (m_DeviceLost)
		{
			return false;
		}

		ID3D12GraphicsCommandList* l_List = m_CommandList.GetHandle();

		const D3D12_RESOURCE_BARRIER l_ToPresent = MakeTransition(m_SwapChain.GetCurrentBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		l_List->ResourceBarrier(1, &l_ToPresent);

		m_GpuTimer.End(l_List, D3D12GpuTimer::k_FrameTimer);
		m_GpuTimer.Resolve(l_List);

		if (!m_CommandList.Close())
		{
			m_DeviceLost = true;

			return false;
		}

		m_FrameFenceValues[m_FrameIndex] = m_DirectQueue.ExecuteCommandList(m_CommandList.GetHandle());

		m_FrameSlotAcquired = false;
		if (!m_SwapChain.Present())
		{
			m_DeviceLost = true;

			return false;
		}

		m_FrameIndex = (m_FrameIndex + 1) % D3D12::k_FramesInFlight;

		const std::chrono::steady_clock::time_point l_Now = std::chrono::steady_clock::now();
		m_Stats.CpuFrameMilliseconds = std::chrono::duration<double, std::milli>(l_Now - m_LastFrameEnd).count();
		m_Stats.GpuFrameMilliseconds = m_GpuTimer.GetMilliseconds(D3D12GpuTimer::k_FrameTimer);
		m_Stats.DrawCalls = m_FrameDrawCalls;
		m_Stats.Triangles = m_FrameTriangles;
		m_LastFrameEnd = l_Now;
		++m_Stats.FrameIndex;

		return true;
	}

	void D3D12Renderer::BeginImGuiFrame()
	{
		PT_CORE_ASSERT(m_InFrame, "BeginImGuiFrame called outside BeginFrame and EndFrame");

		if (!m_InFrame)
		{
			return;
		}

		m_ImGuiPass.BeginFrame();
	}

	void D3D12Renderer::EndImGuiFrame()
	{
		if (!m_InFrame)
		{
			return;
		}

		ID3D12GraphicsCommandList* l_List = m_CommandList.GetHandle();
		m_ImGuiPass.EndFrame(l_List);

		// The DX12 backend binds only the SRV heap; put both heaps back so the frame ends in the state BeginFrame set up
		ID3D12DescriptorHeap* l_Heaps[] = { m_ResourceHeap.GetHandle(), m_SamplerHeap.GetHandle() };
		l_List->SetDescriptorHeaps(2, l_Heaps);

		m_FrameDrawCalls += m_ImGuiPass.GetDrawCallCount();
		m_FrameTriangles += m_ImGuiPass.GetTriangleCount();
	}

	void D3D12Renderer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
		{
			return;
		}

		m_PendingWidth = width;
		m_PendingHeight = height;
		m_ResizePending = true;
	}

	bool D3D12Renderer::ApplyPendingResize()
	{
		m_ResizePending = false;

		// Nothing may still reference the old buffers: finish every frame, then drop what the release queue holds
		m_DirectQueue.Flush();
		m_DeferredRelease.Flush();
		m_FrameFenceValues.fill(0);

		return m_SwapChain.Resize(m_PendingWidth, m_PendingHeight);
	}

	void D3D12Renderer::SetVSync(bool enabled)
	{
		if (enabled == m_SwapChain.IsVSyncEnabled())
		{
			return;
		}

		m_SwapChain.SetVSync(enabled);

		PT_CORE_INFO("VSync {}", enabled ? "on" : "off");
	}

	MeshHandle D3D12Renderer::CreateMesh(const MeshData&)
	{
		PT_CORE_ASSERT(false, "Renderer::CreateMesh arrives with M6");

		return MeshHandle();
	}

	void D3D12Renderer::DestroyMesh(MeshHandle)
	{
		PT_CORE_ASSERT(false, "Renderer::DestroyMesh arrives with M6");
	}

	TextureHandle D3D12Renderer::CreateTexture(const TextureData&)
	{
		PT_CORE_ASSERT(false, "Renderer::CreateTexture arrives with M6");

		return TextureHandle();
	}

	void D3D12Renderer::DestroyTexture(TextureHandle)
	{
		PT_CORE_ASSERT(false, "Renderer::DestroyTexture arrives with M6");
	}

	MaterialHandle D3D12Renderer::CreateMaterial(const MaterialDescription&)
	{
		PT_CORE_ASSERT(false, "Renderer::CreateMaterial arrives with M6");

		return MaterialHandle();
	}

	void D3D12Renderer::UpdateMaterial(MaterialHandle, const MaterialDescription&)
	{
		PT_CORE_ASSERT(false, "Renderer::UpdateMaterial arrives with M6");
	}
}
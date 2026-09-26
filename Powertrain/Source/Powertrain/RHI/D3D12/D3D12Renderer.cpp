#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Platform/Windows/Win32.hpp"
#include "Powertrain/Platform/Windows/WindowsWindow.hpp"
#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/Scene/Scene.hpp"

#include <span>

namespace Powertrain
{
	namespace
	{
		constexpr uint32_t k_RtvCount = 128;
		constexpr uint32_t k_DsvCount = 16;
		constexpr uint32_t k_PersistentResourceCount = 49152;
		constexpr uint32_t k_TransientResourceCountPerFrame = 8192;
		constexpr uint32_t k_SamplerCount = 2048;

		// Constants and per-draw data for one frame; asset uploads go through the copy queue instead
		constexpr uint64_t k_UploadRingBytesPerFrame = 32ull * 1024 * 1024;

		// GPU timer slots; 0 is the frame, 1 the debug lines, the scene passes take 2 onward
		constexpr uint32_t k_DebugDrawTimer = 1;
		constexpr uint32_t k_ForwardTimer = 2;
		constexpr uint32_t k_ShadowTimer = 3;
		constexpr uint32_t k_SkyTimer = 4;
		constexpr uint32_t k_EnvironmentTimer = 5;
		constexpr uint32_t k_PostTimer = 6;

		// Receiver-side shadow bias: how many texels to push along the normal, the constant depth bias in cascade depth units, and the fade over the last stretch of the far cascade
		constexpr float k_ShadowNormalOffsetTexels = 1.5f;
		constexpr float k_ShadowDepthBias = 0.0002f;
		constexpr float k_ShadowFadeLength = 50.0f;

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

		if (!m_Device.GetCapabilities().Bindless)
		{
			PT_CORE_ERROR("Adapter lacks feature level 12_1, Shader Model 6.6 or Resource Binding Tier 3; the bindless renderer cannot start");

			return false;
		}

		if (!m_SceneTarget.Initialize(m_Device, m_RtvHeap, m_ResourceHeap, m_SwapChain.GetWidth(), m_SwapChain.GetHeight(), m_ClearColor))
		{
			return false;
		}

		if (!m_DepthBuffer.Initialize(m_Device, m_DsvHeap, m_SwapChain.GetWidth(), m_SwapChain.GetHeight(), m_SceneTarget.GetSampleCount()))
		{
			return false;
		}

		if (!m_ShadowMap.Initialize(m_Device, m_DsvHeap, m_ResourceHeap))
		{
			return false;
		}

		if (!m_EnvironmentMaps.Initialize(m_Device, m_RtvHeap, m_ResourceHeap))
		{
			return false;
		}

		if (!m_UploadRing.Initialize(m_Device, k_UploadRingBytesPerFrame, "Upload Ring"))
		{
			return false;
		}

		if (!m_PipelineCache.Initialize(m_Device, Win32::GetExecutableDirectory() / "Shaders"))
		{
			return false;
		}

		if (!m_Meshes.Initialize(m_Device, m_CopyQueue, m_ResourceHeap, m_DeferredRelease))
		{
			return false;
		}

		if (!m_Textures.Initialize(m_Device, m_CopyQueue, m_ResourceHeap, m_DeferredRelease))
		{
			return false;
		}

		if (!m_Materials.Initialize())
		{
			return false;
		}

		if (!CreateSamplers())
		{
			return false;
		}

		if (!m_SceneRenderer.Initialize(*this))
		{
			return false;
		}

		// Passes in frame order
		if (!m_EnvironmentPass.Initialize(*this))
		{
			return false;
		}

		if (!m_ShadowPass.Initialize(*this))
		{
			return false;
		}

		if (!m_ForwardPass.Initialize(*this))
		{
			return false;
		}

		if (!m_SkyPass.Initialize(*this))
		{
			return false;
		}

		if (!m_DebugDrawPass.Initialize(*this))
		{
			return false;
		}

		if (!m_PostPass.Initialize(*this))
		{
			return false;
		}

		if (!m_ImGuiPass.Initialize(*this, window))
		{
			return false;
		}

		m_CameraView = CameraView();
		m_FrameConstantsAddress = 0;

		m_FrameFenceValues.fill(0);
		m_FrameIndex = 0;
		m_FrameDrawCalls = 0;
		m_FrameTriangles = 0;
		m_Stats = RendererStats();
		m_Stats.SampleCount = m_SceneTarget.GetSampleCount();
		m_LastFrameEnd = std::chrono::steady_clock::now();
		m_ResizePending = false;
		m_TargetsDirty = false;
		m_InFrame = false;
		m_SceneResolved = false;
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
		m_PostPass.Shutdown();
		m_DebugDrawPass.Shutdown();
		m_SkyPass.Shutdown();
		m_ForwardPass.Shutdown();
		m_ShadowPass.Shutdown();
		m_EnvironmentPass.Shutdown();
		m_SceneRenderer.Shutdown();
		DestroySamplers();
		m_Materials.Shutdown();
		m_Textures.Shutdown();
		m_Meshes.Shutdown();
		m_PipelineCache.Shutdown();
		m_UploadRing.Shutdown();
		m_EnvironmentMaps.Shutdown();
		m_ShadowMap.Shutdown();
		m_DepthBuffer.Shutdown();
		m_SceneTarget.Shutdown();
		m_SwapChain.Shutdown();
		m_SamplerHeap.Shutdown();
		m_ResourceHeap.Shutdown();
		m_DsvHeap.Shutdown();
		m_RtvHeap.Shutdown();
		m_GpuTimer.Shutdown();
		m_CommandList.Shutdown();
		m_CopyQueue.Shutdown();
		m_DirectQueue.Shutdown();
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

		if ((m_ResizePending || m_TargetsDirty) && !ApplyPendingResize())
		{
			m_DeviceLost = true;

			return false;
		}

		// The allocator for this frame index was last used k_FramesInFlight frames ago
		m_DirectQueue.WaitForFence(m_FrameFenceValues[m_FrameIndex]);
		const uint64_t l_Completed = m_DirectQueue.GetCompletedValue();
		m_DeferredRelease.Release(l_Completed);
		m_Meshes.ReleaseCompleted(l_Completed);
		m_Textures.ReleaseCompleted(l_Completed);

		// Same slot, same guarantee
		m_GpuTimer.BeginFrame(m_FrameIndex);

		m_ResourceHeap.BeginFrame(m_FrameIndex);
		m_SamplerHeap.BeginFrame(m_FrameIndex);
		m_UploadRing.BeginFrame(m_FrameIndex);

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

		// The scene colour and depth stay bound through the scene passes and the debug lines; the post pass then takes over the swap chain
		BindSceneTargets(l_List);
		m_SceneTarget.Clear(l_List);
		m_DepthBuffer.Clear(l_List);

		m_FrameDrawCalls = 0;
		m_FrameTriangles = 0;
		m_InFrame = true;
		m_SceneResolved = false;

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

		ResolveScene(l_List);

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
		m_FrameConstantsAddress = 0;

		const std::chrono::steady_clock::time_point l_Now = std::chrono::steady_clock::now();
		m_Stats.CpuFrameMilliseconds = std::chrono::duration<double, std::milli>(l_Now - m_LastFrameEnd).count();
		m_Stats.GpuFrameMilliseconds = m_GpuTimer.GetMilliseconds(D3D12GpuTimer::k_FrameTimer);
		m_Stats.GpuEnvironmentMilliseconds = m_GpuTimer.GetMilliseconds(k_EnvironmentTimer);
		m_Stats.GpuShadowMilliseconds = m_GpuTimer.GetMilliseconds(k_ShadowTimer);
		m_Stats.GpuForwardMilliseconds = m_GpuTimer.GetMilliseconds(k_ForwardTimer);
		m_Stats.GpuSkyMilliseconds = m_GpuTimer.GetMilliseconds(k_SkyTimer);
		m_Stats.GpuDebugDrawMilliseconds = m_GpuTimer.GetMilliseconds(k_DebugDrawTimer);
		m_Stats.GpuPostMilliseconds = m_GpuTimer.GetMilliseconds(k_PostTimer);
		m_Stats.DrawCalls = m_FrameDrawCalls;
		m_Stats.Triangles = m_FrameTriangles;
		m_LastFrameEnd = l_Now;
		++m_Stats.FrameIndex;

		return true;
	}

	void D3D12Renderer::RenderScene(Scene* scene)
	{
		PT_CORE_ASSERT(m_InFrame, "RenderScene called outside BeginFrame and EndFrame");

		if (!m_InFrame)
		{
			return;
		}

		// Camera, culling and the instance upload happen on the CPU first, so the frame constants below carry this frame's view
		m_SceneRenderer.Prepare(scene, m_SwapChain.GetWidth(), m_SwapChain.GetHeight());
		m_CameraView = m_SceneRenderer.GetCameraView();

		// The whole material table is rebuilt into the ring every frame, so UpdateMaterial and texture destruction need no GPU-side bookkeeping
		uint32_t l_MaterialTableIndex = UINT32_MAX;
		const uint32_t l_MaterialCount = m_Materials.GetSlotCount();
		const UploadAllocation l_MaterialAllocation = m_UploadRing.Allocate(static_cast<uint64_t>(l_MaterialCount) * sizeof(ShaderInterop::MaterialData));
		if (l_MaterialAllocation.IsValid())
		{
			m_Materials.WriteTable(m_Textures, std::span<ShaderInterop::MaterialData>(static_cast<ShaderInterop::MaterialData*>(l_MaterialAllocation.Cpu), l_MaterialCount));

			D3D12_SHADER_RESOURCE_VIEW_DESC l_View = {};
			l_View.Format = DXGI_FORMAT_UNKNOWN;
			l_View.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			l_View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			l_View.Buffer.FirstElement = l_MaterialAllocation.Offset / sizeof(ShaderInterop::MaterialData);
			l_View.Buffer.NumElements = l_MaterialCount;
			l_View.Buffer.StructureByteStride = sizeof(ShaderInterop::MaterialData);
			l_View.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

			const DescriptorHandle l_Descriptor = m_ResourceHeap.AllocateTransient();
			if (l_Descriptor.IsValid())
			{
				m_Device.GetHandle()->CreateShaderResourceView(l_MaterialAllocation.Resource, &l_View, l_Descriptor.CPU);
				l_MaterialTableIndex = l_Descriptor.Index;
			}
		}

		// Frame constants go into the ring once; every pass binds the same address at root parameter 1
		const SunLight& l_Sun = m_SceneRenderer.GetSun();
		const float l_Width = static_cast<float>(m_SwapChain.GetWidth());
		const float l_Height = static_cast<float>(m_SwapChain.GetHeight());

		ShaderInterop::FrameConstants l_FrameConstants = {};
		l_FrameConstants.View = m_CameraView.View;
		l_FrameConstants.Projection = m_CameraView.Projection;
		l_FrameConstants.ViewProjection = m_CameraView.ViewProjection;
		l_FrameConstants.InverseViewProjection = m_CameraView.InverseViewProjection;
		l_FrameConstants.CameraPosition = { m_CameraView.Position.X, m_CameraView.Position.Y, m_CameraView.Position.Z, m_CameraView.NearPlane };
		l_FrameConstants.SunDirection = { l_Sun.TowardsSun.X, l_Sun.TowardsSun.Y, l_Sun.TowardsSun.Z, 0.0f };
		l_FrameConstants.SunColor = { l_Sun.Radiance.X, l_Sun.Radiance.Y, l_Sun.Radiance.Z, l_Sun.FromScene ? 1.0f : 0.0f };
		l_FrameConstants.SkyTint = { m_Environment.SkyTint.R, m_Environment.SkyTint.G, m_Environment.SkyTint.B, m_Environment.Exposure };
		l_FrameConstants.ViewportSize = { l_Width, l_Height, 1.0f / l_Width, 1.0f / l_Height };
		l_FrameConstants.FrameInfo.X = static_cast<uint32_t>(m_Stats.FrameIndex);
		l_FrameConstants.FrameInfo.Y = m_SceneRenderer.GetInstanceBufferIndex();
		l_FrameConstants.FrameInfo.Z = l_MaterialTableIndex;
		l_FrameConstants.EnvironmentInfo.X = m_EnvironmentMaps.GetIrradianceIndex();
		l_FrameConstants.EnvironmentInfo.Y = m_EnvironmentMaps.GetSpecularIndex();
		l_FrameConstants.EnvironmentInfo.Z = m_EnvironmentMaps.GetBrdfIndex();
		l_FrameConstants.EnvironmentInfo.W = D3D12EnvironmentMaps::k_SpecularMipCount;

		// Shadow constants: cascade matrices, where each ends, how big its texels are, and the receiver bias
		const std::span<const ShadowCascade> l_Cascades = m_SceneRenderer.GetShadowCascades();
		float l_Splits[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float l_TexelSizes[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		for (size_t l_Index = 0; l_Index < l_Cascades.size(); ++l_Index)
		{
			l_FrameConstants.ShadowMatrices[l_Index] = l_Cascades[l_Index].ViewProjection;
			l_Splits[l_Index] = l_Cascades[l_Index].SplitDistance;
			l_TexelSizes[l_Index] = l_Cascades[l_Index].TexelWorldSize;
		}
		const float l_ShadowFar = l_Cascades.empty() ? 0.0f : l_Cascades.back().SplitDistance;
		l_FrameConstants.ShadowSplits = { l_Splits[0], l_Splits[1], l_Splits[2], l_Splits[3] };
		l_FrameConstants.ShadowTexelSizes = { l_TexelSizes[0], l_TexelSizes[1], l_TexelSizes[2], l_TexelSizes[3] };
		l_FrameConstants.ShadowParams = { k_ShadowNormalOffsetTexels, k_ShadowDepthBias, l_ShadowFar - k_ShadowFadeLength, k_ShadowFadeLength };
		l_FrameConstants.ShadowInfo.X = m_ShadowMap.GetShaderResourceIndex();
		l_FrameConstants.ShadowInfo.Y = static_cast<uint32_t>(l_Cascades.size());
		l_FrameConstants.ShadowInfo.Z = D3D12ShadowMap::k_Size;
		m_FrameConstantsAddress = m_UploadRing.Upload(l_FrameConstants).Gpu;

		// Without a material table the shader would index past the heap, so the frame draws nothing rather than faulting
		const bool l_CanDraw = l_MaterialTableIndex != UINT32_MAX;
		const std::span<const DrawBatch> l_Batches = l_CanDraw ? m_SceneRenderer.GetBatches() : std::span<const DrawBatch>();
		const std::span<const ShadowCascade> l_ShadowCascades = l_CanDraw ? l_Cascades : std::span<const ShadowCascade>();

		ID3D12GraphicsCommandList* l_List = m_CommandList.GetHandle();

		// The sky is baked into the environment maps only when the sun or the tint moved; the forward pass reads them later in this list
		if (m_EnvironmentPass.NeedsBake(l_Sun, m_Environment.SkyTint))
		{
			m_GpuTimer.Begin(l_List, k_EnvironmentTimer);
			m_EnvironmentPass.Render(l_List, m_EnvironmentMaps, m_UploadRing, m_FrameConstantsAddress, l_Sun, m_Environment.SkyTint);
			m_GpuTimer.End(l_List, k_EnvironmentTimer);

			m_FrameDrawCalls += m_EnvironmentPass.GetDrawCallCount();
		}

		// Shadows next, into their own depth slices; then the scene targets come back for the forward pass and the sky
		m_GpuTimer.Begin(l_List, k_ShadowTimer);
		m_ShadowPass.Render(l_List, m_ShadowMap, m_UploadRing, m_FrameConstantsAddress, l_ShadowCascades);
		m_GpuTimer.End(l_List, k_ShadowTimer);

		BindSceneTargets(l_List);

		m_GpuTimer.Begin(l_List, k_ForwardTimer);
		m_ForwardPass.Render(l_List, m_FrameConstantsAddress, l_Batches);
		m_GpuTimer.End(l_List, k_ForwardTimer);

		m_GpuTimer.Begin(l_List, k_SkyTimer);
		m_SkyPass.Render(l_List, m_FrameConstantsAddress);
		m_GpuTimer.End(l_List, k_SkyTimer);

		m_FrameDrawCalls += m_ShadowPass.GetDrawCallCount() + m_ForwardPass.GetDrawCallCount() + m_SkyPass.GetDrawCallCount();
		m_FrameTriangles += m_ShadowPass.GetTriangleCount() + m_ForwardPass.GetTriangleCount();
		m_Stats.ShadowDrawCalls = m_ShadowPass.GetDrawCallCount();
		m_Stats.EnvironmentBakes = m_EnvironmentPass.GetBakeCount();
		m_Stats.Instances = m_SceneRenderer.GetVisibleInstanceCount();
		m_Stats.CulledInstances = m_SceneRenderer.GetCulledInstanceCount();
		m_Stats.Meshes = m_Meshes.GetAliveCount();
		m_Stats.Textures = m_Textures.GetAliveCount();
		m_Stats.Materials = m_Materials.GetAliveCount();
		m_Stats.GpuMemoryBytes = m_Meshes.GetGpuBytes() + m_Textures.GetGpuBytes();
	}

	void D3D12Renderer::RenderDebugDraw()
	{
		PT_CORE_ASSERT(m_InFrame, "RenderDebugDraw called outside BeginFrame and EndFrame");

		if (!m_InFrame)
		{
			return;
		}

		ID3D12GraphicsCommandList* l_List = m_CommandList.GetHandle();

		m_GpuTimer.Begin(l_List, k_DebugDrawTimer);
		m_DebugDrawPass.Render(l_List, m_FrameConstantsAddress);
		m_GpuTimer.End(l_List, k_DebugDrawTimer);

		m_FrameDrawCalls += m_DebugDrawPass.GetDrawCallCount();
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

		// The scene lands on the swap chain first, so the UI draws over the tonemapped image
		ResolveScene(l_List);

		// The ImGui pipeline has no depth format, so only the swap chain view is bound for its draws
		const D3D12_CPU_DESCRIPTOR_HANDLE l_Rtv = m_SwapChain.GetCurrentRtv();
		l_List->OMSetRenderTargets(1, &l_Rtv, FALSE, nullptr);
		l_List->OMSetRenderTargets(1, &l_Rtv, FALSE, nullptr);

		m_ImGuiPass.EndFrame(l_List);

		// The DX12 backend binds only the SRV heap
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
		// A clear colour change alone recreates the scene target at the current size, because its optimized clear value is baked in
		const uint32_t l_Width = m_ResizePending ? m_PendingWidth : m_SwapChain.GetWidth();
		const uint32_t l_Height = m_ResizePending ? m_PendingHeight : m_SwapChain.GetHeight();
		m_ResizePending = false;
		m_TargetsDirty = false;

		// Nothing may still reference the old buffers: finish every frame, then drop what the release queue holds
		m_DirectQueue.Flush();
		m_DeferredRelease.Flush();
		m_Meshes.ReleaseCompleted(m_DirectQueue.GetCompletedValue());
		m_Textures.ReleaseCompleted(m_DirectQueue.GetCompletedValue());
		m_FrameFenceValues.fill(0);

		return m_SwapChain.Resize(l_Width, l_Height) && m_SceneTarget.Resize(l_Width, l_Height, m_ClearColor) && m_DepthBuffer.Resize(l_Width, l_Height);
	}

	void D3D12Renderer::SetClearColor(const Color& color)
	{
		m_ClearColor = color;

		// The scene target carries the colour as its optimized clear value, so it is rebuilt at the next BeginFrame
		m_TargetsDirty = true;
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

	MeshHandle D3D12Renderer::CreateMesh(const MeshData& data)
	{
		if (!m_Initialized || m_DeviceLost)
		{
			return MeshHandle();
		}

		return m_Meshes.Create(data);
	}

	void D3D12Renderer::DestroyMesh(MeshHandle mesh)
	{
		if (!m_Initialized)
		{
			return;
		}

		// The frame being recorded signals last + 1 in EndFrame, and a Flush signals at least that, so the buffers outlive every reader
		m_Meshes.Destroy(mesh, m_DirectQueue.GetLastSignaledValue() + 1);
	}

	TextureHandle D3D12Renderer::CreateTexture(const TextureData& data)
	{
		if (!m_Initialized || m_DeviceLost)
		{
			return TextureHandle();
		}

		return m_Textures.Create(data);
	}

	void D3D12Renderer::DestroyTexture(TextureHandle texture)
	{
		if (!m_Initialized)
		{
			return;
		}

		// Same fence rule as meshes: the frame being recorded signals last + 1 in EndFrame
		m_Textures.Destroy(texture, m_DirectQueue.GetLastSignaledValue() + 1);
	}

	MaterialHandle D3D12Renderer::CreateMaterial(const MaterialDescription& description)
	{
		if (!m_Initialized)
		{
			return MaterialHandle();
		}

		return m_Materials.Create(description);
	}

	void D3D12Renderer::UpdateMaterial(MaterialHandle material, const MaterialDescription& description)
	{
		if (!m_Initialized)
		{
			return;
		}

		m_Materials.Update(material, description);
	}

	void D3D12Renderer::DestroyMaterial(MaterialHandle material)
	{
		if (!m_Initialized)
		{
			return;
		}

		// Materials are CPU data rebuilt into the ring each frame, so nothing on the GPU outlives this call
		m_Materials.Destroy(material);
	}

	void D3D12Renderer::BindSceneTargets(ID3D12GraphicsCommandList* commandList)
	{
		// The multisampled scene colour and depth stay bound through the scene passes and the debug lines; the post pass moves to the swap chain
		const D3D12_CPU_DESCRIPTOR_HANDLE l_Rtv = m_SceneTarget.GetRtv();
		const D3D12_CPU_DESCRIPTOR_HANDLE l_Dsv = m_DepthBuffer.GetDsv();
		commandList->OMSetRenderTargets(1, &l_Rtv, FALSE, &l_Dsv);

		D3D12_VIEWPORT l_Viewport = {};
		l_Viewport.Width = static_cast<float>(m_SwapChain.GetWidth());
		l_Viewport.Height = static_cast<float>(m_SwapChain.GetHeight());
		l_Viewport.MinDepth = 0.0f;
		l_Viewport.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &l_Viewport);

		const D3D12_RECT l_Scissor = { 0, 0, static_cast<LONG>(m_SwapChain.GetWidth()), static_cast<LONG>(m_SwapChain.GetHeight()) };
		commandList->RSSetScissorRects(1, &l_Scissor);
	}

	void D3D12Renderer::ResolveScene(ID3D12GraphicsCommandList* commandList)
	{
		if (m_SceneResolved)
		{
			return;
		}

		// Resolve and tonemap once per frame, from whichever of EndImGuiFrame and EndFrame comes first
		m_GpuTimer.Begin(commandList, k_PostTimer);
		m_PostPass.Render(commandList, m_SceneTarget, m_UploadRing, m_SwapChain.GetCurrentRtv(), m_Environment.Exposure);
		m_GpuTimer.End(commandList, k_PostTimer);

		m_FrameDrawCalls += m_PostPass.GetDrawCallCount();
		m_SceneResolved = true;
	}

	bool D3D12Renderer::CreateSamplers()
	{
		std::array<D3D12_SAMPLER_DESC, ShaderInterop::k_SamplerCount> l_Descriptions = {};

		for (D3D12_SAMPLER_DESC& l_Description : l_Descriptions)
		{
			l_Description.MipLODBias = 0.0f;
			l_Description.MaxAnisotropy = 1;
			l_Description.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			l_Description.MinLOD = 0.0f;
			l_Description.MaxLOD = D3D12_FLOAT32_MAX;
		}

		l_Descriptions[ShaderInterop::k_SamplerLinearWrap].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		l_Descriptions[ShaderInterop::k_SamplerLinearWrap].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		l_Descriptions[ShaderInterop::k_SamplerLinearWrap].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		l_Descriptions[ShaderInterop::k_SamplerLinearWrap].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

		l_Descriptions[ShaderInterop::k_SamplerLinearClamp].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		l_Descriptions[ShaderInterop::k_SamplerLinearClamp].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		l_Descriptions[ShaderInterop::k_SamplerLinearClamp].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		l_Descriptions[ShaderInterop::k_SamplerLinearClamp].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

		l_Descriptions[ShaderInterop::k_SamplerAnisotropicWrap].Filter = D3D12_FILTER_ANISOTROPIC;
		l_Descriptions[ShaderInterop::k_SamplerAnisotropicWrap].MaxAnisotropy = 8;
		l_Descriptions[ShaderInterop::k_SamplerAnisotropicWrap].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		l_Descriptions[ShaderInterop::k_SamplerAnisotropicWrap].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		l_Descriptions[ShaderInterop::k_SamplerAnisotropicWrap].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

		l_Descriptions[ShaderInterop::k_SamplerPointClamp].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		l_Descriptions[ShaderInterop::k_SamplerPointClamp].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		l_Descriptions[ShaderInterop::k_SamplerPointClamp].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		l_Descriptions[ShaderInterop::k_SamplerPointClamp].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

		// PCF taps: bilinear comparison, lit when the receiver's depth is at or above the stored depth under reversed-Z
		l_Descriptions[ShaderInterop::k_SamplerShadow].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		l_Descriptions[ShaderInterop::k_SamplerShadow].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		l_Descriptions[ShaderInterop::k_SamplerShadow].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		l_Descriptions[ShaderInterop::k_SamplerShadow].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		l_Descriptions[ShaderInterop::k_SamplerShadow].ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

		// The shaders index the heap with the ShaderInterop constants, so these must be the first slots the heap hands out
		for (uint32_t l_Index = 0; l_Index < ShaderInterop::k_SamplerCount; ++l_Index)
		{
			m_Samplers[l_Index] = m_SamplerHeap.Allocate();
			if (!m_Samplers[l_Index].IsValid() || m_Samplers[l_Index].Index != l_Index)
			{
				PT_CORE_ERROR("Sampler slot {} landed at descriptor {}; the sampler heap must be empty when the renderer starts", l_Index, m_Samplers[l_Index].Index);

				return false;
			}

			m_Device.GetHandle()->CreateSampler(&l_Descriptions[l_Index], m_Samplers[l_Index].CPU);
		}

		return true;
	}

	void D3D12Renderer::DestroySamplers()
	{
		for (DescriptorHandle& l_Sampler : m_Samplers)
		{
			m_SamplerHeap.Free(l_Sampler);
		}
	}
}
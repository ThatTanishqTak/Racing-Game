#pragma once

#include "Powertrain/Renderer/DebugDrawPass.hpp"
#include "Powertrain/Renderer/ForwardPass.hpp"
#include "Powertrain/Renderer/ImGuiPass.hpp"
#include "Powertrain/Renderer/Renderer.hpp"
#include "Powertrain/RHI/D3D12/D3D12CommandList.hpp"
#include "Powertrain/RHI/D3D12/D3D12CommandQueue.hpp"
#include "Powertrain/RHI/D3D12/D3D12DeferredReleaseQueue.hpp"
#include "Powertrain/RHI/D3D12/D3D12DescriptorHeap.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"
#include "Powertrain/RHI/D3D12/D3D12GpuTimer.hpp"
#include "Powertrain/RHI/D3D12/D3D12PipelineCache.hpp"
#include "Powertrain/RHI/D3D12/D3D12SwapChain.hpp"
#include "Powertrain/RHI/D3D12/D3D12UploadRing.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>

namespace Powertrain
{
	class WindowsWindow;

	class D3D12Renderer final : public Renderer
	{
	public:
		D3D12Renderer();
		~D3D12Renderer() override;

		D3D12Renderer(const D3D12Renderer&) = delete;
		D3D12Renderer& operator=(const D3D12Renderer&) = delete;

		bool Initialize(WindowsWindow& window, const RendererSettings& settings);
		void Shutdown();

		// Frame flow, driven by ApplicationImplementation
		void WaitForNextFrame();
		bool BeginFrame();
		bool EndFrame();

		// The scene passes, before the layers' OnRender so their debug lines land on top
		void RenderScene();

		// Draws and clears the frame's debug lines
		void RenderDebugDraw();

		// ImGui frame, between OnRender and OnImGuiRender on the layers
		void BeginImGuiFrame();
		void EndImGuiFrame();

		// Deferred to the next BeginFrame, so it is safe from any layer hook
		void Resize(uint32_t width, uint32_t height);

		// Renderer
		MeshHandle CreateMesh(const MeshData& data) override;
		void DestroyMesh(MeshHandle mesh) override;
		TextureHandle CreateTexture(const TextureData& data) override;
		void DestroyTexture(TextureHandle texture) override;
		MaterialHandle CreateMaterial(const MaterialDescription& description) override;
		void UpdateMaterial(MaterialHandle material, const MaterialDescription& description) override;

		void SetVSync(bool enabled) override;
		bool IsVSyncEnabled() const override { return m_SwapChain.IsVSyncEnabled(); }
		void SetClearColor(const Color& color) override { m_ClearColor = color; }
		void SetEnvironment(const EnvironmentSettings& environment) override { m_Environment = environment; }
		void SetViewProjection(const Matrix4& viewProjection) override { m_ViewProjection = viewProjection; }

		DebugDraw& GetDebugDraw() override { return m_DebugDrawPass; }
		const RendererStats& GetStats() const override { return m_Stats; }
		std::string_view GetAdapterName() const override { return m_Device.GetCapabilities().AdapterName; }

		// For the passes
		D3D12Device& GetDevice() { return m_Device; }
		D3D12CommandQueue& GetDirectQueue() { return m_DirectQueue; }
		D3D12CommandQueue& GetCopyQueue() { return m_CopyQueue; }
		D3D12CommandList& GetCommandList() { return m_CommandList; }
		D3D12SwapChain& GetSwapChain() { return m_SwapChain; }
		D3D12DescriptorHeap& GetRtvHeap() { return m_RtvHeap; }
		D3D12DescriptorHeap& GetDsvHeap() { return m_DsvHeap; }
		D3D12DescriptorHeap& GetResourceHeap() { return m_ResourceHeap; }
		D3D12DescriptorHeap& GetSamplerHeap() { return m_SamplerHeap; }
		D3D12DeferredReleaseQueue& GetDeferredReleaseQueue() { return m_DeferredRelease; }
		D3D12GpuTimer& GetGpuTimer() { return m_GpuTimer; }
		D3D12UploadRing& GetUploadRing() { return m_UploadRing; }
		D3D12PipelineCache& GetPipelineCache() { return m_PipelineCache; }
		uint32_t GetFrameIndex() const { return m_FrameIndex; }
		const EnvironmentSettings& GetEnvironment() const { return m_Environment; }

		// Root CBV address of this frame's FrameConstants in the upload ring; zero outside a frame
		D3D12_GPU_VIRTUAL_ADDRESS GetFrameConstantsAddress() const { return m_FrameConstantsAddress; }

	private:
		bool ApplyPendingResize();

	private:
		D3D12Device m_Device;
		D3D12CommandQueue m_DirectQueue;
		D3D12CommandQueue m_CopyQueue;
		D3D12CommandList m_CommandList;
		D3D12DescriptorHeap m_RtvHeap;
		D3D12DescriptorHeap m_DsvHeap;
		D3D12DescriptorHeap m_ResourceHeap;
		D3D12DescriptorHeap m_SamplerHeap;
		D3D12SwapChain m_SwapChain;
		D3D12DeferredReleaseQueue m_DeferredRelease;
		D3D12GpuTimer m_GpuTimer;
		D3D12UploadRing m_UploadRing;
		D3D12PipelineCache m_PipelineCache;
		ForwardPass m_ForwardPass;
		DebugDrawPass m_DebugDrawPass;
		ImGuiPass m_ImGuiPass;

		uint32_t m_FrameDrawCalls = 0;
		uint32_t m_FrameTriangles = 0;
		RendererStats m_Stats;
		EnvironmentSettings m_Environment;
		Matrix4 m_ViewProjection;
		Color m_ClearColor;
		D3D12_GPU_VIRTUAL_ADDRESS m_FrameConstantsAddress = 0;

		std::array<uint64_t, D3D12::k_FramesInFlight> m_FrameFenceValues = {};
		uint32_t m_FrameIndex = 0;
		std::chrono::steady_clock::time_point m_LastFrameEnd;

		uint32_t m_PendingWidth = 0;
		uint32_t m_PendingHeight = 0;
		bool m_ResizePending = false;

		bool m_Initialized = false;
		bool m_InFrame = false;
		bool m_FrameSlotAcquired = false;
		bool m_DeviceLost = false;
	};
}
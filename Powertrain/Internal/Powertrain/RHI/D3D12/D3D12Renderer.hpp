#pragma once

#include "Powertrain/Renderer/Camera.hpp"
#include "Powertrain/Renderer/DebugDrawPass.hpp"
#include "Powertrain/Renderer/EnvironmentPass.hpp"
#include "Powertrain/Renderer/ForwardPass.hpp"
#include "Powertrain/Renderer/ImGuiPass.hpp"
#include "Powertrain/Renderer/PostPass.hpp"
#include "Powertrain/Renderer/Renderer.hpp"
#include "Powertrain/Renderer/SceneRenderer.hpp"
#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/Renderer/ShadowPass.hpp"
#include "Powertrain/Renderer/SkyPass.hpp"
#include "Powertrain/RHI/D3D12/D3D12CommandList.hpp"
#include "Powertrain/RHI/D3D12/D3D12CommandQueue.hpp"
#include "Powertrain/RHI/D3D12/D3D12DeferredReleaseQueue.hpp"
#include "Powertrain/RHI/D3D12/D3D12DepthBuffer.hpp"
#include "Powertrain/RHI/D3D12/D3D12DescriptorHeap.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"
#include "Powertrain/RHI/D3D12/D3D12EnvironmentMaps.hpp"
#include "Powertrain/RHI/D3D12/D3D12GpuTimer.hpp"
#include "Powertrain/RHI/D3D12/D3D12MaterialStorage.hpp"
#include "Powertrain/RHI/D3D12/D3D12MeshStorage.hpp"
#include "Powertrain/RHI/D3D12/D3D12PipelineCache.hpp"
#include "Powertrain/RHI/D3D12/D3D12SceneTarget.hpp"
#include "Powertrain/RHI/D3D12/D3D12ShadowMap.hpp"
#include "Powertrain/RHI/D3D12/D3D12SwapChain.hpp"
#include "Powertrain/RHI/D3D12/D3D12TextureStorage.hpp"
#include "Powertrain/RHI/D3D12/D3D12UploadRing.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>

namespace Powertrain
{
	class Scene;
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

		// Camera, culling and the scene passes for the active scene (null draws nothing)
		void RenderScene(Scene* scene);

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
		void DestroyMaterial(MaterialHandle material) override;

		void SetVSync(bool enabled) override;
		bool IsVSyncEnabled() const override { return m_SwapChain.IsVSyncEnabled(); }
		void SetClearColor(const Color& color) override;
		void SetEnvironment(const EnvironmentSettings& environment) override { m_Environment = environment; }

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
		D3D12SceneTarget& GetSceneTarget() { return m_SceneTarget; }
		D3D12DepthBuffer& GetDepthBuffer() { return m_DepthBuffer; }
		D3D12ShadowMap& GetShadowMap() { return m_ShadowMap; }
		D3D12EnvironmentMaps& GetEnvironmentMaps() { return m_EnvironmentMaps; }
		D3D12MeshStorage& GetMeshStorage() { return m_Meshes; }
		const D3D12MeshStorage& GetMeshStorage() const { return m_Meshes; }
		D3D12TextureStorage& GetTextureStorage() { return m_Textures; }
		const D3D12TextureStorage& GetTextureStorage() const { return m_Textures; }
		D3D12MaterialStorage& GetMaterialStorage() { return m_Materials; }
		const D3D12MaterialStorage& GetMaterialStorage() const { return m_Materials; }
		uint32_t GetFrameIndex() const { return m_FrameIndex; }

		const EnvironmentSettings& GetEnvironment() const { return m_Environment; }
		const CameraView& GetCameraView() const { return m_CameraView; }

		D3D12_GPU_VIRTUAL_ADDRESS GetFrameConstantsAddress() const { return m_FrameConstantsAddress; }

	private:
		bool ApplyPendingResize();
		bool CreateSamplers();
		void DestroySamplers();
		void BindSceneTargets(ID3D12GraphicsCommandList* commandList);
		void ResolveScene(ID3D12GraphicsCommandList* commandList);

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
		D3D12SceneTarget m_SceneTarget;
		D3D12DepthBuffer m_DepthBuffer;
		D3D12ShadowMap m_ShadowMap;
		D3D12EnvironmentMaps m_EnvironmentMaps;
		D3D12UploadRing m_UploadRing;
		D3D12PipelineCache m_PipelineCache;
		D3D12MeshStorage m_Meshes;
		D3D12TextureStorage m_Textures;
		D3D12MaterialStorage m_Materials;
		std::array<DescriptorHandle, ShaderInterop::k_SamplerCount> m_Samplers;
		SceneRenderer m_SceneRenderer;
		EnvironmentPass m_EnvironmentPass;
		ShadowPass m_ShadowPass;
		ForwardPass m_ForwardPass;
		SkyPass m_SkyPass;
		DebugDrawPass m_DebugDrawPass;
		PostPass m_PostPass;
		ImGuiPass m_ImGuiPass;


		uint32_t m_FrameDrawCalls = 0;
		uint32_t m_FrameTriangles = 0;
		RendererStats m_Stats;
		EnvironmentSettings m_Environment;
		CameraView m_CameraView;
		Color m_ClearColor;
		D3D12_GPU_VIRTUAL_ADDRESS m_FrameConstantsAddress = 0;

		std::array<uint64_t, D3D12::k_FramesInFlight> m_FrameFenceValues = {};
		uint32_t m_FrameIndex = 0;
		std::chrono::steady_clock::time_point m_LastFrameEnd;

		uint32_t m_PendingWidth = 0;
		uint32_t m_PendingHeight = 0;
		bool m_ResizePending = false;
		bool m_TargetsDirty = false;

		bool m_Initialized = false;
		bool m_InFrame = false;
		bool m_SceneResolved = false;
		bool m_FrameSlotAcquired = false;
		bool m_DeviceLost = false;
	};
}
#pragma once

#include "Powertrain/Math/Matrix4.hpp"
#include "Powertrain/Renderer/Camera.hpp"
#include "Powertrain/Renderer/ForwardPass.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace Powertrain
{
	class D3D12Renderer;
	class Scene;

	class SceneRenderer
	{
	public:
		SceneRenderer() = default;
		~SceneRenderer() = default;

		SceneRenderer(const SceneRenderer&) = delete;
		SceneRenderer& operator=(const SceneRenderer&) = delete;

		bool Initialize(D3D12Renderer& renderer);
		void Shutdown();
		void Prepare(Scene* scene, uint32_t viewportWidth, uint32_t viewportHeight);

		const CameraView& GetCameraView() const { return m_Camera; }
		std::span<const DrawBatch> GetBatches() const { return m_Batches; }

		uint32_t GetInstanceBufferIndex() const { return m_InstanceBufferIndex; }
		uint32_t GetVisibleInstanceCount() const { return m_VisibleCount; }
		uint32_t GetCulledInstanceCount() const { return m_CulledCount; }

	private:
		struct VisibleInstance
		{
			uint32_t MeshSlot = 0;
			const D3D12Mesh* Mesh = nullptr;
			Matrix4 World;
		};

		void ResolveCamera(Scene* scene, float aspectRatio);
		void GatherInstances(Scene* scene);
		void UploadInstances();

	private:
		D3D12Renderer* m_Renderer = nullptr;

		CameraView m_Camera;
		std::vector<VisibleInstance> m_Visible;
		std::vector<DrawBatch> m_Batches;

		uint32_t m_InstanceBufferIndex = UINT32_MAX;
		uint32_t m_VisibleCount = 0;
		uint32_t m_CulledCount = 0;

		bool m_WarnedNoCamera = false;
		bool m_Initialized = false;
	};
}
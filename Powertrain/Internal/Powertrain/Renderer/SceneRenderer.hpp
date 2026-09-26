#pragma once

#include "Powertrain/Math/Matrix4.hpp"
#include "Powertrain/Renderer/Camera.hpp"
#include "Powertrain/Renderer/ForwardPass.hpp"
#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/Renderer/ShadowPass.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace Powertrain
{
	class D3D12Renderer;
	class Scene;

	struct SunLight
	{
		Vector3 TowardsSun = Vector3::Up();
		Vector3 Radiance = Vector3::One();
		bool CastShadows = true;
		bool FromScene = false;
	};

	class SceneRenderer
	{
	public:
		static constexpr uint32_t k_ShadowCascadeCount = ShaderInterop::k_ShadowCascadeCount;

		SceneRenderer() = default;
		~SceneRenderer() = default;

		SceneRenderer(const SceneRenderer&) = delete;
		SceneRenderer& operator=(const SceneRenderer&) = delete;

		bool Initialize(D3D12Renderer& renderer);
		void Shutdown();
		void Prepare(Scene* scene, uint32_t viewportWidth, uint32_t viewportHeight);

		const CameraView& GetCameraView() const { return m_Camera; }
		const SunLight& GetSun() const { return m_Sun; }

		std::span<const DrawBatch> GetBatches() const { return m_Batches; }
		std::span<const ShadowCascade> GetShadowCascades() const { return std::span<const ShadowCascade>(m_ShadowCascades.data(), m_ShadowCascadeCount); }

		uint32_t GetShadowCascadeCount() const { return m_ShadowCascadeCount; }
		uint32_t GetInstanceBufferIndex() const { return m_InstanceBufferIndex; }
		uint32_t GetVisibleInstanceCount() const { return m_VisibleCount; }
		uint32_t GetCulledInstanceCount() const { return m_CulledCount; }
		uint32_t GetShadowInstanceCount() const { return m_ShadowInstanceCount; }

	private:
		struct GatheredInstance
		{
			uint32_t MeshSlot = 0;
			uint32_t MaterialSlot = 0;
			const D3D12Mesh* Mesh = nullptr;
			Matrix4 World;
			uint32_t VisibilityMask = 0;
		};

		static constexpr uint32_t k_CameraBit = 1u;
		static constexpr uint32_t CascadeBit(uint32_t cascade) { return 1u << (cascade + 1); }

		void ResolveCamera(Scene* scene, float aspectRatio);
		void ResolveSun(Scene* scene);
		void ResolveShadows();
		void GatherInstances(Scene* scene);
		void UploadInstances();

		static void AppendToBatches(std::vector<DrawBatch>& batches, const GatheredInstance& instance, uint32_t instanceIndex);

	private:
		D3D12Renderer* m_Renderer = nullptr;

		CameraView m_Camera;
		SunLight m_Sun;
		std::array<ShadowCascade, k_ShadowCascadeCount> m_ShadowCascades;
		uint32_t m_ShadowCascadeCount = 0;

		std::vector<GatheredInstance> m_Instances;
		std::vector<DrawBatch> m_Batches;

		uint32_t m_InstanceBufferIndex = UINT32_MAX;
		uint32_t m_VisibleCount = 0;
		uint32_t m_CulledCount = 0;
		uint32_t m_ShadowInstanceCount = 0;

		bool m_WarnedNoCamera = false;
		bool m_Initialized = false;
	};
}
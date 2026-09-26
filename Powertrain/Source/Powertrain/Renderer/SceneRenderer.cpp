#include "Powertrain/Renderer/SceneRenderer.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/RHI/D3D12/D3D12MeshStorage.hpp"
#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"
#include "Powertrain/Scene/Components.hpp"
#include "Powertrain/Scene/Scene.hpp"

#include <algorithm>

namespace Powertrain
{
	namespace
	{
		constexpr Vector3 k_DefaultEye = { 0.0f, 6.0f, 14.0f };
		constexpr Vector3 k_DefaultTarget = { 0.0f, 1.0f, 0.0f };
		constexpr float k_DefaultFovDegrees = 60.0f;
		constexpr float k_DefaultNearPlane = 0.1f;
	}

	bool SceneRenderer::Initialize(D3D12Renderer& renderer)
	{
		m_Renderer = &renderer;
		m_Visible.reserve(1024);
		m_Batches.reserve(64);
		m_WarnedNoCamera = false;
		m_Initialized = true;

		PT_CORE_INFO("Scene renderer ready");

		return true;
	}

	void SceneRenderer::Shutdown()
	{
		m_Visible.clear();
		m_Batches.clear();
		m_Renderer = nullptr;

		if (m_Initialized)
		{
			PT_CORE_INFO("Scene renderer shut down");
		}

		m_Initialized = false;
	}

	void SceneRenderer::Prepare(Scene* scene, uint32_t viewportWidth, uint32_t viewportHeight)
	{
		m_Visible.clear();
		m_Batches.clear();
		m_InstanceBufferIndex = UINT32_MAX;
		m_VisibleCount = 0;
		m_CulledCount = 0;

		if (!m_Initialized)
		{
			return;
		}

		const float l_Aspect = viewportHeight != 0 ? static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight) : 1.0f;
		ResolveCamera(scene, l_Aspect);

		if (scene == nullptr)
		{
			return;
		}

		GatherInstances(scene);
		UploadInstances();
	}

	void SceneRenderer::ResolveCamera(Scene* scene, float aspectRatio)
	{
		const CameraComponent* l_Camera = nullptr;
		const WorldTransformComponent* l_World = nullptr;

		if (scene != nullptr)
		{
			const Entity l_Entity = scene->GetPrimaryCamera();
			if (scene->IsAlive(l_Entity))
			{
				l_Camera = scene->GetRegistry().TryGet<CameraComponent>(l_Entity);
				l_World = scene->GetRegistry().TryGet<WorldTransformComponent>(l_Entity);
			}
		}

		if (l_Camera != nullptr && l_World != nullptr)
		{
			m_Camera.View = l_World->World.Inverse();
			m_Camera.Position = l_World->World.GetTranslation();
			m_Camera.NearPlane = l_Camera->NearPlane;
			m_Camera.Projection = Matrix4::Perspective(Math::ToRadians(l_Camera->VerticalFovDegrees), aspectRatio, l_Camera->NearPlane);
			m_WarnedNoCamera = false;
		}
		else
		{
			if (!m_WarnedNoCamera)
			{
				PT_CORE_WARN("Active scene has no primary camera with CameraComponent and WorldTransformComponent; using the default view");
				m_WarnedNoCamera = true;
			}

			m_Camera.View = Matrix4::LookAt(k_DefaultEye, k_DefaultTarget, Vector3::Up());
			m_Camera.Position = k_DefaultEye;
			m_Camera.NearPlane = k_DefaultNearPlane;
			m_Camera.Projection = Matrix4::Perspective(Math::ToRadians(k_DefaultFovDegrees), aspectRatio, k_DefaultNearPlane);
		}

		m_Camera.ViewProjection = m_Camera.View * m_Camera.Projection;
		m_Camera.Frustum = Frustum::FromViewProjection(m_Camera.ViewProjection);
	}

	void SceneRenderer::GatherInstances(Scene* scene)
	{
		const D3D12MeshStorage& l_Meshes = m_Renderer->GetMeshStorage();
		const Frustum& l_Frustum = m_Camera.Frustum;

		scene->GetRegistry().Each<WorldTransformComponent, MeshRendererComponent>([this, &l_Meshes, &l_Frustum](Entity, const WorldTransformComponent& world, const MeshRendererComponent& meshRenderer)
		{
			const D3D12Mesh* l_Mesh = l_Meshes.Get(meshRenderer.Mesh);
			if (l_Mesh == nullptr)
			{
				return;
			}

			if (!l_Frustum.Intersects(l_Mesh->Bounds.Transformed(world.World)))
			{
				++m_CulledCount;

				return;
			}

			m_Visible.push_back({ meshRenderer.Mesh.Index, l_Mesh, world.World });
		});

		std::sort(m_Visible.begin(), m_Visible.end(), [](const VisibleInstance& a, const VisibleInstance& b)
		{
			return a.MeshSlot < b.MeshSlot;
		});
	}

	void SceneRenderer::UploadInstances()
	{
		if (m_Visible.empty())
		{
			return;
		}

		D3D12UploadRing& l_Ring = m_Renderer->GetUploadRing();
		const UploadAllocation l_Allocation = l_Ring.Allocate(m_Visible.size() * sizeof(ShaderInterop::InstanceData));
		if (!l_Allocation.IsValid())
		{
			m_Visible.clear();

			return;
		}

		ShaderInterop::InstanceData* l_Instances = static_cast<ShaderInterop::InstanceData*>(l_Allocation.Cpu);
		for (size_t l_Index = 0; l_Index < m_Visible.size(); ++l_Index)
		{
			l_Instances[l_Index].World = m_Visible[l_Index].World;

			if (m_Batches.empty() || m_Batches.back().Mesh != m_Visible[l_Index].Mesh)
			{
				m_Batches.push_back({ m_Visible[l_Index].Mesh, static_cast<uint32_t>(l_Index), 1 });
			}
			else
			{
				++m_Batches.back().InstanceCount;
			}
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC l_View = {};
		l_View.Format = DXGI_FORMAT_UNKNOWN;
		l_View.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		l_View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		l_View.Buffer.FirstElement = l_Allocation.Offset / sizeof(ShaderInterop::InstanceData);
		l_View.Buffer.NumElements = static_cast<UINT>(m_Visible.size());
		l_View.Buffer.StructureByteStride = sizeof(ShaderInterop::InstanceData);
		l_View.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		const DescriptorHandle l_Descriptor = m_Renderer->GetResourceHeap().AllocateTransient();
		if (!l_Descriptor.IsValid())
		{
			m_Visible.clear();
			m_Batches.clear();

			return;
		}

		m_Renderer->GetDevice().GetHandle()->CreateShaderResourceView(l_Allocation.Resource, &l_View, l_Descriptor.CPU);

		m_InstanceBufferIndex = l_Descriptor.Index;
		m_VisibleCount = static_cast<uint32_t>(m_Visible.size());
	}
}
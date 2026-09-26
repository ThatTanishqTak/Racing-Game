#include "Powertrain/Renderer/SceneRenderer.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12MaterialStorage.hpp"
#include "Powertrain/RHI/D3D12/D3D12MeshStorage.hpp"
#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"
#include "Powertrain/RHI/D3D12/D3D12ShadowMap.hpp"
#include "Powertrain/Scene/Components.hpp"
#include "Powertrain/Scene/Scene.hpp"

#include <algorithm>
#include <cmath>

namespace Powertrain
{
	namespace
	{
		constexpr Vector3 k_DefaultEye = { 0.0f, 6.0f, 14.0f };
		constexpr Vector3 k_DefaultTarget = { 0.0f, 1.0f, 0.0f };
		constexpr float k_DefaultFovDegrees = 60.0f;
		constexpr float k_DefaultNearPlane = 0.1f;
		constexpr float k_ShadowMaxDistance = 500.0f;
		constexpr float k_ShadowSplitLambda = 0.75f;
		constexpr float k_ShadowSplitNear = 1.0f;
		constexpr float k_ShadowCasterMargin = 200.0f;
		constexpr float k_ShadowNearPlane = 1.0f;
	}

	bool SceneRenderer::Initialize(D3D12Renderer& renderer)
	{
		m_Renderer = &renderer;
		m_Instances.reserve(1024);
		m_Batches.reserve(64);
		for (ShadowCascade& l_Cascade : m_ShadowCascades)
		{
			l_Cascade.Batches.reserve(64);
		}

		m_WarnedNoCamera = false;
		m_Initialized = true;

		PT_CORE_INFO("Scene renderer ready");

		return true;
	}

	void SceneRenderer::Shutdown()
	{
		m_Instances.clear();
		m_Batches.clear();
		for (ShadowCascade& l_Cascade : m_ShadowCascades)
		{
			l_Cascade.Batches.clear();
		}

		m_Renderer = nullptr;

		if (m_Initialized)
		{
			PT_CORE_INFO("Scene renderer shut down");
		}

		m_Initialized = false;
	}

	void SceneRenderer::Prepare(Scene* scene, uint32_t viewportWidth, uint32_t viewportHeight)
	{
		m_Instances.clear();
		m_Batches.clear();
		for (ShadowCascade& l_Cascade : m_ShadowCascades)
		{
			l_Cascade.Batches.clear();
		}

		m_ShadowCascadeCount = 0;
		m_InstanceBufferIndex = UINT32_MAX;
		m_VisibleCount = 0;
		m_CulledCount = 0;
		m_ShadowInstanceCount = 0;

		if (!m_Initialized)
		{
			return;
		}

		const float l_Aspect = viewportHeight != 0 ? static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight) : 1.0f;
		ResolveCamera(scene, l_Aspect);
		ResolveSun(scene);
		ResolveShadows();

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

		m_Camera.AspectRatio = aspectRatio;

		if (l_Camera != nullptr && l_World != nullptr)
		{
			m_Camera.View = l_World->World.Inverse();
			m_Camera.Position = l_World->World.GetTranslation();
			m_Camera.NearPlane = l_Camera->NearPlane;
			m_Camera.VerticalFov = Math::ToRadians(l_Camera->VerticalFovDegrees);
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
			m_Camera.VerticalFov = Math::ToRadians(k_DefaultFovDegrees);
		}

		m_Camera.Projection = Matrix4::Perspective(m_Camera.VerticalFov, aspectRatio, m_Camera.NearPlane);
		m_Camera.ViewProjection = m_Camera.View * m_Camera.Projection;
		m_Camera.Frustum = Frustum::FromViewProjection(m_Camera.ViewProjection);
	}

	void SceneRenderer::ResolveSun(Scene* scene)
	{
		bool l_Found = false;

		if (scene != nullptr)
		{
			// The first light wins; the entity's -Z is where the light travels, so +Z points back towards the sun
			scene->GetRegistry().Each<WorldTransformComponent, DirectionalLightComponent>([this, &l_Found](Entity, const WorldTransformComponent& world, const DirectionalLightComponent& light)
			{
				if (l_Found)
				{
					return;
				}

				m_Sun.TowardsSun = world.World.GetAxis(2).Normalized();
				m_Sun.Radiance = { light.Color.R * light.Intensity, light.Color.G * light.Intensity, light.Color.B * light.Intensity };
				m_Sun.CastShadows = light.CastShadows;
				m_Sun.FromScene = true;
				l_Found = true;
			});
		}

		if (!l_Found)
		{
			const EnvironmentSettings& l_Environment = m_Renderer->GetEnvironment();
			m_Sun.TowardsSun = (-l_Environment.SunDirection).Normalized();
			m_Sun.Radiance = { l_Environment.SunColor.R * l_Environment.SunIntensity, l_Environment.SunColor.G * l_Environment.SunIntensity, l_Environment.SunColor.B * l_Environment.SunIntensity };
			m_Sun.CastShadows = true;
			m_Sun.FromScene = false;
		}
	}

	void SceneRenderer::ResolveShadows()
	{
		if (!m_Sun.CastShadows)
		{
			return;
		}

		const float l_SplitNear = std::max(m_Camera.NearPlane, k_ShadowSplitNear);
		const float l_SplitFar = k_ShadowMaxDistance;
		const Matrix4 l_CameraWorld = m_Camera.View.Inverse();
		const float l_TanHalfFov = std::tan(m_Camera.VerticalFov * 0.5f);
		const float l_Size = static_cast<float>(D3D12ShadowMap::k_Size);

		// Any up that is not parallel to the sun keeps the light view well defined at noon
		Vector3 l_Up = Vector3::Up();
		if (Math::Abs(Vector3::Dot(m_Sun.TowardsSun, l_Up)) > 0.99f)
		{
			l_Up = Vector3::UnitZ();
		}

		float l_SliceNear = m_Camera.NearPlane;
		for (uint32_t l_Index = 0; l_Index < k_ShadowCascadeCount; ++l_Index)
		{
			ShadowCascade& l_Cascade = m_ShadowCascades[l_Index];

			const float l_Fraction = static_cast<float>(l_Index + 1) / static_cast<float>(k_ShadowCascadeCount);
			const float l_Uniform = l_SplitNear + (l_SplitFar - l_SplitNear) * l_Fraction;
			const float l_Logarithmic = l_SplitNear * std::pow(l_SplitFar / l_SplitNear, l_Fraction);
			const float l_SliceFar = Math::Lerp(l_Uniform, l_Logarithmic, k_ShadowSplitLambda);

			// The eight corners of this slice of the view frustum, in world space
			std::array<Vector3, 8> l_Corners;
			Vector3 l_Center = Vector3::Zero();
			for (uint32_t l_Corner = 0; l_Corner < 8; ++l_Corner)
			{
				const float l_Distance = (l_Corner & 4) ? l_SliceFar : l_SliceNear;
				const float l_HalfHeight = l_Distance * l_TanHalfFov;
				const float l_HalfWidth = l_HalfHeight * m_Camera.AspectRatio;

				l_Corners[l_Corner] = l_CameraWorld.TransformPoint({ (l_Corner & 1) ? l_HalfWidth : -l_HalfWidth, (l_Corner & 2) ? l_HalfHeight : -l_HalfHeight, -l_Distance });
				l_Center += l_Corners[l_Corner];
			}
			l_Center = l_Center / 8.0f;

			// A bounding sphere keeps the orthographic size constant as the camera turns, so texels never change size between frames
			float l_Radius = 0.0f;
			for (const Vector3& l_Corner : l_Corners)
			{
				l_Radius = std::max(l_Radius, Vector3::Distance(l_Center, l_Corner));
			}

			const float l_Extent = l_Radius * 2.0f;
			const Vector3 l_Eye = l_Center + m_Sun.TowardsSun * (l_Radius + k_ShadowCasterMargin);
			const Matrix4 l_View = Matrix4::LookAt(l_Eye, l_Center, l_Up);
			Matrix4 l_Projection = Matrix4::Orthographic(l_Extent, l_Extent, k_ShadowNearPlane, l_Extent + k_ShadowCasterMargin + k_ShadowNearPlane);

			const Vector4 l_Origin = Vector4::Point(Vector3::Zero()) * (l_View * l_Projection);
			const float l_HalfSize = l_Size * 0.5f;
			l_Projection.M[3][0] += std::round(l_Origin.X * l_HalfSize) / l_HalfSize - l_Origin.X;
			l_Projection.M[3][1] += std::round(l_Origin.Y * l_HalfSize) / l_HalfSize - l_Origin.Y;

			l_Cascade.ViewProjection = l_View * l_Projection;
			l_Cascade.SplitDistance = l_SliceFar;
			l_Cascade.TexelWorldSize = l_Extent / l_Size;
			l_Cascade.Frustum = Frustum::FromViewProjection(l_Cascade.ViewProjection);

			l_SliceNear = l_SliceFar;
		}

		m_ShadowCascadeCount = k_ShadowCascadeCount;
	}

	void SceneRenderer::GatherInstances(Scene* scene)
	{
		const D3D12MeshStorage& l_Meshes = m_Renderer->GetMeshStorage();
		const D3D12MaterialStorage& l_Materials = m_Renderer->GetMaterialStorage();

		scene->GetRegistry().Each<WorldTransformComponent, MeshRendererComponent>([this, &l_Meshes, &l_Materials](Entity, const WorldTransformComponent& world, const MeshRendererComponent& meshRenderer)
		{
			const D3D12Mesh* l_Mesh = l_Meshes.Get(meshRenderer.Mesh);
			if (l_Mesh == nullptr)
			{
				return;
			}

			const BoundingBox l_Bounds = l_Mesh->Bounds.Transformed(world.World);

			uint32_t l_Mask = 0;
			if (m_Camera.Frustum.Intersects(l_Bounds))
			{
				l_Mask |= k_CameraBit;
			}

			if (meshRenderer.CastShadows)
			{
				for (uint32_t l_Index = 0; l_Index < m_ShadowCascadeCount; ++l_Index)
				{
					if (m_ShadowCascades[l_Index].Frustum.Intersects(l_Bounds))
					{
						l_Mask |= CascadeBit(l_Index);
					}
				}
			}

			if (l_Mask == 0)
			{
				++m_CulledCount;

				return;
			}

			m_Instances.push_back({ meshRenderer.Mesh.Index, l_Materials.GetSlot(meshRenderer.Material), l_Mesh, world.World, l_Mask });
		});

		// Same mesh and material together, so a batch is one DrawIndexedInstanced over a contiguous instance range
		std::sort(m_Instances.begin(), m_Instances.end(), [](const GatheredInstance& a, const GatheredInstance& b)
		{
			return a.MeshSlot != b.MeshSlot ? a.MeshSlot < b.MeshSlot : a.MaterialSlot < b.MaterialSlot;
		});
	}

	void SceneRenderer::UploadInstances()
	{
		if (m_Instances.empty())
		{
			return;
		}

		D3D12UploadRing& l_Ring = m_Renderer->GetUploadRing();
		const UploadAllocation l_Allocation = l_Ring.Allocate(m_Instances.size() * sizeof(ShaderInterop::InstanceData));
		if (!l_Allocation.IsValid())
		{
			m_Instances.clear();

			return;
		}

		// One upload for every pass; each list only references the instances its frustum kept, so a run breaks wherever a bit is missing
		ShaderInterop::InstanceData* l_Data = static_cast<ShaderInterop::InstanceData*>(l_Allocation.Cpu);
		for (size_t l_Index = 0; l_Index < m_Instances.size(); ++l_Index)
		{
			const GatheredInstance& l_Instance = m_Instances[l_Index];
			l_Data[l_Index].World = l_Instance.World;

			const uint32_t l_InstanceIndex = static_cast<uint32_t>(l_Index);
			if (l_Instance.VisibilityMask & k_CameraBit)
			{
				AppendToBatches(m_Batches, l_Instance, l_InstanceIndex);
				++m_VisibleCount;
			}

			for (uint32_t l_Cascade = 0; l_Cascade < m_ShadowCascadeCount; ++l_Cascade)
			{
				if (l_Instance.VisibilityMask & CascadeBit(l_Cascade))
				{
					AppendToBatches(m_ShadowCascades[l_Cascade].Batches, l_Instance, l_InstanceIndex);
					++m_ShadowInstanceCount;
				}
			}
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC l_View = {};
		l_View.Format = DXGI_FORMAT_UNKNOWN;
		l_View.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		l_View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		l_View.Buffer.FirstElement = l_Allocation.Offset / sizeof(ShaderInterop::InstanceData);
		l_View.Buffer.NumElements = static_cast<UINT>(m_Instances.size());
		l_View.Buffer.StructureByteStride = sizeof(ShaderInterop::InstanceData);
		l_View.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		const DescriptorHandle l_Descriptor = m_Renderer->GetResourceHeap().AllocateTransient();
		if (!l_Descriptor.IsValid())
		{
			m_Instances.clear();
			m_Batches.clear();
			for (ShadowCascade& l_Cascade : m_ShadowCascades)
			{
				l_Cascade.Batches.clear();
			}

			m_VisibleCount = 0;
			m_ShadowInstanceCount = 0;

			return;
		}

		m_Renderer->GetDevice().GetHandle()->CreateShaderResourceView(l_Allocation.Resource, &l_View, l_Descriptor.CPU);

		m_InstanceBufferIndex = l_Descriptor.Index;
	}

	void SceneRenderer::AppendToBatches(std::vector<DrawBatch>& batches, const GatheredInstance& instance, uint32_t instanceIndex)
	{
		if (!batches.empty())
		{
			DrawBatch& l_Last = batches.back();
			if (l_Last.Mesh == instance.Mesh && l_Last.MaterialIndex == instance.MaterialSlot && l_Last.FirstInstance + l_Last.InstanceCount == instanceIndex)
			{
				++l_Last.InstanceCount;

				return;
			}
		}

		batches.push_back({ instance.Mesh, instance.MaterialSlot, instanceIndex, 1 });
	}
}
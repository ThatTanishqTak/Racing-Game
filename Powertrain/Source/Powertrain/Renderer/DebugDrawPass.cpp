#include "Powertrain/Renderer/DebugDrawPass.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/RHI/D3D12/D3D12DepthBuffer.hpp"
#include "Powertrain/RHI/D3D12/D3D12PipelineCache.hpp"
#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"
#include "Powertrain/RHI/D3D12/D3D12SceneTarget.hpp"
#include "Powertrain/RHI/D3D12/D3D12UploadRing.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>


namespace Powertrain
{
	namespace
	{
		constexpr uint32_t k_SphereSegments = 32;
		constexpr float k_ArrowHeadFraction = 0.2f;
		constexpr float k_Epsilon = Math::k_Epsilon;

		// RGBA8 with R in the low byte; DebugLine.hlsl unpacks in the same order
		uint32_t PackColor(const Color& color)
		{
			const auto l_Channel = [](float value)
			{
				return static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
			};

			return l_Channel(color.R) | (l_Channel(color.G) << 8) | (l_Channel(color.B) << 16) | (l_Channel(color.A) << 24);
		}
	}

	DebugDrawPass::~DebugDrawPass()
	{
		Shutdown();
	}

	bool DebugDrawPass::Initialize(D3D12Renderer& renderer)
	{
		m_Renderer = &renderer;

		D3D12PipelineCache& l_Cache = renderer.GetPipelineCache();
		m_RootSignature = l_Cache.GetRootSignature();

		// Lines into the multisampled scene target: straight alpha blend, no culling, depth-tested against the scene without writing
		GraphicsPipelineDescription l_Description;
		l_Description.VertexShader = "DebugLine.VSMain";
		l_Description.PixelShader = "DebugLine.PSMain";
		l_Description.RenderTargetFormats[0] = D3D12SceneTarget::k_Format;
		l_Description.RenderTargetCount = 1;
		l_Description.DepthFormat = D3D12DepthBuffer::k_Format;
		l_Description.SampleCount = renderer.GetSceneTarget().GetSampleCount();
		l_Description.Topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		l_Description.CullMode = D3D12_CULL_MODE_NONE;
		l_Description.DepthTest = true;
		l_Description.DepthWrite = false;
		l_Description.Blend = BlendMode::AlphaBlend;

		m_Pipeline = l_Cache.GetGraphicsPipeline(l_Description);
		if (m_Pipeline == nullptr)
		{
			Shutdown();

			return false;
		}

		m_Vertices.reserve(4096);
		m_DroppedVertices = 0;
		m_DrawCalls = 0;
		m_OverflowWarned = false;
		m_Initialized = true;

		PT_CORE_INFO("Debug draw ready: {} vertices per frame", k_MaxVertices);

		return true;
	}

	void DebugDrawPass::Shutdown()
	{
		m_Pipeline = nullptr;
		m_RootSignature = nullptr;
		m_Renderer = nullptr;

		m_Vertices.clear();
		m_DroppedVertices = 0;
		m_DrawCalls = 0;

		if (m_Initialized)
		{
			PT_CORE_INFO("Debug draw shut down");
		}

		m_Initialized = false;
	}

	void DebugDrawPass::Render(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS frameConstants)
	{
		m_DrawCalls = 0;


		if (m_DroppedVertices != 0)
		{
			if (!m_OverflowWarned)
			{
				PT_CORE_WARN("Debug draw dropped {} lines this frame; raise DebugDrawPass::k_MaxVertices", m_DroppedVertices / 2);
				m_OverflowWarned = true;
			}
		}
		else
		{
			m_OverflowWarned = false;
		}

		// The lines need the frame's view-projection, which RenderScene wrote; without it there is nothing to draw with
		if (m_Initialized && !m_Vertices.empty() && frameConstants != 0)
		{
			const UploadAllocation l_Allocation = m_Renderer->GetUploadRing().Allocate(m_Vertices.size() * sizeof(LineVertex));
			const DescriptorHandle l_Descriptor = l_Allocation.IsValid() ? m_Renderer->GetResourceHeap().AllocateTransient() : DescriptorHandle();
			if (l_Descriptor.IsValid())
			{
				std::memcpy(l_Allocation.Cpu, m_Vertices.data(), m_Vertices.size() * sizeof(LineVertex));

				// The ring slice becomes a structured buffer the shader reaches through the descriptor in the root constants
				D3D12_SHADER_RESOURCE_VIEW_DESC l_View = {};
				l_View.Format = DXGI_FORMAT_UNKNOWN;
				l_View.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				l_View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				l_View.Buffer.FirstElement = l_Allocation.Offset / sizeof(LineVertex);
				l_View.Buffer.NumElements = static_cast<UINT>(m_Vertices.size());
				l_View.Buffer.StructureByteStride = sizeof(LineVertex);
				l_View.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				m_Renderer->GetDevice().GetHandle()->CreateShaderResourceView(l_Allocation.Resource, &l_View, l_Descriptor.CPU);

				ShaderInterop::DrawConstants l_Draw = {};
				l_Draw.VertexBufferIndex = l_Descriptor.Index;

				commandList->SetGraphicsRootSignature(m_RootSignature);
				commandList->SetPipelineState(m_Pipeline);
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
				commandList->SetGraphicsRootConstantBufferView(D3D12PipelineCache::k_FrameConstantsParameter, frameConstants);
				commandList->SetGraphicsRoot32BitConstants(D3D12PipelineCache::k_DrawConstantsParameter, D3D12PipelineCache::k_DrawConstantCount, &l_Draw, 0);
				commandList->DrawInstanced(static_cast<UINT>(m_Vertices.size()), 1, 0, 0);

				m_DrawCalls = 1;
			}
		}


		m_Vertices.clear();
		m_DroppedVertices = 0;
	}

	void DebugDrawPass::Line(const Vector3& from, const Vector3& to, const Color& color)
	{
		Push(from, to, PackColor(color));
	}

	void DebugDrawPass::Arrow(const Vector3& origin, const Vector3& vector, const Color& color)
	{
		const uint32_t l_Color = PackColor(color);
		const Vector3 l_Tip = origin + vector;
		Push(origin, l_Tip, l_Color);

		const float l_Length = vector.Length();
		if (l_Length <= k_Epsilon)
		{
			return;
		}

		// Four head lines from the tip back to a square around the shaft
		const Vector3 l_Direction = vector / l_Length;
		const float l_HeadLength = l_Length * k_ArrowHeadFraction;
		const float l_HeadWidth = l_HeadLength * 0.5f;
		const Vector3 l_Base = l_Tip - l_Direction * l_HeadLength;
		const Vector3 l_Perpendicular = l_Direction.Perpendicular();
		const Vector3 l_Side = l_Perpendicular * l_HeadWidth;
		const Vector3 l_Up = Vector3::Cross(l_Direction, l_Perpendicular) * l_HeadWidth;

		Push(l_Tip, l_Base + l_Side, l_Color);
		Push(l_Tip, l_Base - l_Side, l_Color);
		Push(l_Tip, l_Base + l_Up, l_Color);
		Push(l_Tip, l_Base - l_Up, l_Color);
	}

	void DebugDrawPass::Box(const Matrix4& transform, const Vector3& halfExtents, const Color& color)
	{
		const uint32_t l_Color = PackColor(color);

		// Corner i has bit 0 for +X, bit 1 for +Y, bit 2 for +Z
		Vector3 l_Corners[8];
		for (uint32_t l_Index = 0; l_Index < 8; ++l_Index)
		{
			const Vector3 l_Local =
			{
				(l_Index & 1) ? halfExtents.X : -halfExtents.X,
				(l_Index & 2) ? halfExtents.Y : -halfExtents.Y,
				(l_Index & 4) ? halfExtents.Z : -halfExtents.Z
			};

			l_Corners[l_Index] = transform.TransformPoint(l_Local);
		}

		// Each edge joins two corners that differ in exactly one bit; walking from the 0 side gives every edge once
		for (uint32_t l_Index = 0; l_Index < 8; ++l_Index)
		{
			for (uint32_t l_Bit = 0; l_Bit < 3; ++l_Bit)
			{
				const uint32_t l_Other = l_Index | (1u << l_Bit);
				if (l_Other != l_Index)
				{
					Push(l_Corners[l_Index], l_Corners[l_Other], l_Color);
				}
			}
		}
	}

	void DebugDrawPass::Sphere(const Vector3& center, float radius, const Color& color)
	{
		const uint32_t l_Color = PackColor(color);
		const float l_Step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(k_SphereSegments);

		// Three great circles, one per axis pair
		const Vector3 l_Axes[3][2] =
		{
			{ { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
		};

		for (const auto& l_Pair : l_Axes)
		{
			Vector3 l_Previous = center + l_Pair[0] * radius;
			for (uint32_t l_Segment = 1; l_Segment <= k_SphereSegments; ++l_Segment)
			{
				const float l_Angle = l_Step * static_cast<float>(l_Segment);
				const Vector3 l_Point = center + l_Pair[0] * (radius * std::cos(l_Angle)) + l_Pair[1] * (radius * std::sin(l_Angle));

				Push(l_Previous, l_Point, l_Color);
				l_Previous = l_Point;
			}
		}
	}

	void DebugDrawPass::Push(const Vector3& from, const Vector3& to, uint32_t color)
	{
		if (m_Vertices.size() + 2 > k_MaxVertices)
		{
			m_DroppedVertices += 2;

			return;
		}

		m_Vertices.push_back({ { from.X, from.Y, from.Z }, color });
		m_Vertices.push_back({ { to.X, to.Y, to.Z }, color });
	}
}
#include "Powertrain/Renderer/DebugDrawPass.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <numbers>

namespace Powertrain
{
	namespace
	{
		constexpr uint32_t k_SphereSegments = 32;
		constexpr float k_ArrowHeadFraction = 0.2f;
		constexpr float k_Epsilon = Math::k_Epsilon;

		// Matrix4 is uploaded raw as 16 root constants
		static_assert(sizeof(Matrix4) == 16 * sizeof(float), "Matrix4 must be 16 floats");

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
		ID3D12Device* l_Device = renderer.GetDevice().GetHandle();

		// The cache reads and keeps the bytecode; the pass builds its own pipeline until it joins the global root signature at M6 step 5
		const std::vector<uint8_t>* l_VertexShader = renderer.GetPipelineCache().GetShader("DebugLine.VSMain");
		const std::vector<uint8_t>* l_PixelShader = renderer.GetPipelineCache().GetShader("DebugLine.PSMain");
		if (l_VertexShader == nullptr || l_PixelShader == nullptr)
		{
			return false;
		}

		if (!CreateRootSignature(l_Device) || !CreatePipelineState(l_Device, *l_VertexShader, *l_PixelShader) || !CreateVertexBuffers(l_Device))
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
		for (FrameBuffer& l_Frame : m_VertexBuffers)
		{
			if (l_Frame.Mapped != nullptr)
			{
				l_Frame.Buffer->Unmap(0, nullptr);
				l_Frame.Mapped = nullptr;
			}

			l_Frame.Buffer.Reset();
		}

		m_PipelineState.Reset();
		m_RootSignature.Reset();

		m_Vertices.clear();
		m_DroppedVertices = 0;
		m_DrawCalls = 0;

		if (m_Initialized)
		{
			PT_CORE_INFO("Debug draw shut down");
		}

		m_Initialized = false;
	}

	void DebugDrawPass::Render(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex, const Matrix4& viewProjection)
	{
		PT_CORE_ASSERT(frameIndex < D3D12::k_FramesInFlight, "Frame index {} out of range", frameIndex);

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

		if (m_Initialized && !m_Vertices.empty())
		{
			// The renderer waited on this frame index's fence in BeginFrame, so the GPU is done reading this buffer
			FrameBuffer& l_Frame = m_VertexBuffers[frameIndex];
			std::memcpy(l_Frame.Mapped, m_Vertices.data(), m_Vertices.size() * sizeof(LineVertex));

			commandList->SetGraphicsRootSignature(m_RootSignature.Get());
			commandList->SetPipelineState(m_PipelineState.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			commandList->SetGraphicsRoot32BitConstants(0, 16, &viewProjection, 0);
			commandList->SetGraphicsRootShaderResourceView(1, l_Frame.Buffer->GetGPUVirtualAddress());
			commandList->DrawInstanced(static_cast<UINT>(m_Vertices.size()), 1, 0, 0);

			m_DrawCalls = 1;
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

	bool DebugDrawPass::CreateRootSignature(ID3D12Device* device)
	{
		// 0: 16 root constants for the view-projection, 1: root SRV for the vertex buffer; both read by the vertex shader only
		D3D12_ROOT_PARAMETER l_Parameters[2] = {};
		l_Parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		l_Parameters[0].Constants.ShaderRegister = 0;
		l_Parameters[0].Constants.RegisterSpace = 0;
		l_Parameters[0].Constants.Num32BitValues = 16;
		l_Parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		l_Parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		l_Parameters[1].Descriptor.ShaderRegister = 0;
		l_Parameters[1].Descriptor.RegisterSpace = 0;
		l_Parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		D3D12_ROOT_SIGNATURE_DESC l_Description = {};
		l_Description.NumParameters = 2;
		l_Description.pParameters = l_Parameters;
		l_Description.NumStaticSamplers = 0;
		l_Description.pStaticSamplers = nullptr;
		l_Description.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		ComPtr<ID3DBlob> l_Serialized;
		ComPtr<ID3DBlob> l_Error;
		const HRESULT l_Result = D3D12SerializeRootSignature(&l_Description, D3D_ROOT_SIGNATURE_VERSION_1, &l_Serialized, &l_Error);
		if (FAILED(l_Result))
		{
			if (l_Error)
			{
				PT_CORE_ERROR("Debug draw root signature: {}", static_cast<const char*>(l_Error->GetBufferPointer()));
			}

			return D3D12::CheckResult(l_Result, "D3D12SerializeRootSignature (debug draw)");
		}

		if (!D3D12::CheckResult(device->CreateRootSignature(0, l_Serialized->GetBufferPointer(), l_Serialized->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)), "CreateRootSignature (debug draw)"))
		{
			return false;
		}

		D3D12::SetDebugName(m_RootSignature.Get(), "Debug Draw Root Signature");

		return true;
	}

	bool DebugDrawPass::CreatePipelineState(ID3D12Device* device, const std::vector<uint8_t>& vertexShader, const std::vector<uint8_t>& pixelShader)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC l_Description = {};
		l_Description.pRootSignature = m_RootSignature.Get();
		l_Description.VS = { vertexShader.data(), vertexShader.size() };
		l_Description.PS = { pixelShader.data(), pixelShader.size() };

		// Straight alpha blend so translucent lines work; colours are linear and the sRGB RTV encodes on write
		D3D12_RENDER_TARGET_BLEND_DESC& l_Blend = l_Description.BlendState.RenderTarget[0];
		l_Blend.BlendEnable = TRUE;
		l_Blend.LogicOpEnable = FALSE;
		l_Blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		l_Blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		l_Blend.BlendOp = D3D12_BLEND_OP_ADD;
		l_Blend.SrcBlendAlpha = D3D12_BLEND_ONE;
		l_Blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		l_Blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		l_Blend.LogicOp = D3D12_LOGIC_OP_NOOP;
		l_Blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		l_Description.BlendState.AlphaToCoverageEnable = FALSE;
		l_Description.BlendState.IndependentBlendEnable = FALSE;
		l_Description.SampleMask = UINT_MAX;

		l_Description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		l_Description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		l_Description.RasterizerState.FrontCounterClockwise = FALSE;
		l_Description.RasterizerState.DepthClipEnable = TRUE;
		l_Description.RasterizerState.MultisampleEnable = FALSE;
		l_Description.RasterizerState.AntialiasedLineEnable = FALSE;
		l_Description.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		// No depth until M6 gives the pass the MSAA depth target
		l_Description.DepthStencilState.DepthEnable = FALSE;
		l_Description.DepthStencilState.StencilEnable = FALSE;

		// Vertex pulling: no input layout
		l_Description.InputLayout = { nullptr, 0 };
		l_Description.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		l_Description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		l_Description.NumRenderTargets = 1;
		l_Description.RTVFormats[0] = D3D12SwapChain::k_RtvFormat;
		l_Description.DSVFormat = DXGI_FORMAT_UNKNOWN;
		l_Description.SampleDesc = { 1, 0 };
		l_Description.NodeMask = 0;

		if (!D3D12::CheckResult(device->CreateGraphicsPipelineState(&l_Description, IID_PPV_ARGS(&m_PipelineState)), "CreateGraphicsPipelineState (debug draw)"))
		{
			return false;
		}

		D3D12::SetDebugName(m_PipelineState.Get(), "Debug Draw Pipeline");

		return true;
	}

	bool DebugDrawPass::CreateVertexBuffers(ID3D12Device* device)
	{
		D3D12_HEAP_PROPERTIES l_HeapProperties = {};
		l_HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC l_BufferDescription = {};
		l_BufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		l_BufferDescription.Width = static_cast<uint64_t>(k_MaxVertices) * sizeof(LineVertex);
		l_BufferDescription.Height = 1;
		l_BufferDescription.DepthOrArraySize = 1;
		l_BufferDescription.MipLevels = 1;
		l_BufferDescription.Format = DXGI_FORMAT_UNKNOWN;
		l_BufferDescription.SampleDesc = { 1, 0 };
		l_BufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		l_BufferDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

		for (uint32_t l_Index = 0; l_Index < D3D12::k_FramesInFlight; ++l_Index)
		{
			FrameBuffer& l_Frame = m_VertexBuffers[l_Index];

			if (!D3D12::CheckResult(device->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_BufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&l_Frame.Buffer)), std::format("CreateCommittedResource (debug draw vertices {})", l_Index)))
			{
				return false;
			}

			D3D12::SetDebugName(l_Frame.Buffer.Get(), std::format("Debug Draw Vertices {}", l_Index));

			// Upload heaps stay mapped for their whole life; the CPU never reads them back
			const D3D12_RANGE l_NoRead = { 0, 0 };
			void* l_Mapped = nullptr;
			if (!D3D12::CheckResult(l_Frame.Buffer->Map(0, &l_NoRead, &l_Mapped), "ID3D12Resource::Map (debug draw vertices)"))
			{
				return false;
			}

			l_Frame.Mapped = static_cast<LineVertex*>(l_Mapped);
		}

		return true;
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
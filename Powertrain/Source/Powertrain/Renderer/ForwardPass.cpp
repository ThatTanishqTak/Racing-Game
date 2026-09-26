#include "Powertrain/Renderer/ForwardPass.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Renderer/ShaderInterop.hpp"
#include "Powertrain/RHI/D3D12/D3D12PipelineCache.hpp"
#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"

#include <array>
#include <cstring>

namespace Powertrain
{
	ForwardPass::~ForwardPass()
	{
		Shutdown();
	}

	bool ForwardPass::Initialize(D3D12Renderer& renderer)
	{
		D3D12PipelineCache& l_Cache = renderer.GetPipelineCache();
		m_ResourceHeap = &renderer.GetResourceHeap();
		m_RootSignature = l_Cache.GetRootSignature();

		GraphicsPipelineDescription l_Description;
		l_Description.VertexShader = "Forward.VSMain";
		l_Description.PixelShader = "Forward.PSMain";
		l_Description.RenderTargetFormats[0] = D3D12SwapChain::k_RtvFormat;
		l_Description.RenderTargetCount = 1;
		l_Description.CullMode = D3D12_CULL_MODE_NONE;

		m_Pipeline = l_Cache.GetGraphicsPipeline(l_Description);
		if (m_Pipeline == nullptr || !CreateTriangle(renderer))
		{
			Shutdown();

			return false;
		}

		m_DrawCalls = 0;
		m_Triangles = 0;
		m_Initialized = true;

		PT_CORE_INFO("Forward pass ready");

		return true;
	}

	void ForwardPass::Shutdown()
	{
		if (m_ResourceHeap != nullptr)
		{
			m_ResourceHeap->Free(m_TriangleView);
		}

		m_TriangleVertices.Reset();
		m_TriangleVertexCount = 0;
		m_Pipeline = nullptr;
		m_RootSignature = nullptr;
		m_ResourceHeap = nullptr;
		m_DrawCalls = 0;
		m_Triangles = 0;

		if (m_Initialized)
		{
			PT_CORE_INFO("Forward pass shut down");
		}

		m_Initialized = false;
	}

	void ForwardPass::Render(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS frameConstants)
	{
		m_DrawCalls = 0;
		m_Triangles = 0;

		if (!m_Initialized || frameConstants == 0)
		{
			return;
		}

		commandList->SetGraphicsRootSignature(m_RootSignature);
		commandList->SetPipelineState(m_Pipeline);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->SetGraphicsRootConstantBufferView(D3D12PipelineCache::k_FrameConstantsParameter, frameConstants);

		// The only per-draw state is which buffer to pull from; the vertex shader indexes the heap with it
		ShaderInterop::DrawConstants l_Draw = {};
		l_Draw.VertexBufferIndex = m_TriangleView.Index;
		commandList->SetGraphicsRoot32BitConstants(D3D12PipelineCache::k_DrawConstantsParameter, D3D12PipelineCache::k_DrawConstantCount, &l_Draw, 0);
		commandList->DrawInstanced(m_TriangleVertexCount, 1, 0, 0);

		m_DrawCalls = 1;
		m_Triangles = m_TriangleVertexCount / 3;
	}

	bool ForwardPass::CreateTriangle(D3D12Renderer& renderer)
	{
		ID3D12Device* l_Device = renderer.GetDevice().GetHandle();

		// Standing on the grid opposite the Sandbox sphere, counter-clockwise seen from +Z; texcoords give the three colours
		const std::array<Vertex, 3> l_Vertices =
		{
			Vertex{ { -6.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, 1.0f, { 0.0f, 0.0f } },
			Vertex{ { -3.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, 1.0f, { 1.0f, 0.0f } },
			Vertex{ { -5.0f, 2.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, 1.0f, { 0.0f, 1.0f } }
		};
		const uint64_t l_Bytes = l_Vertices.size() * sizeof(Vertex);

		// An upload heap is enough for 144 bytes read once a frame; CreateMesh moves real meshes to default heaps
		D3D12_HEAP_PROPERTIES l_HeapProperties = {};
		l_HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC l_BufferDescription = {};
		l_BufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		l_BufferDescription.Width = l_Bytes;
		l_BufferDescription.Height = 1;
		l_BufferDescription.DepthOrArraySize = 1;
		l_BufferDescription.MipLevels = 1;
		l_BufferDescription.Format = DXGI_FORMAT_UNKNOWN;
		l_BufferDescription.SampleDesc = { 1, 0 };
		l_BufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		l_BufferDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

		if (!D3D12::CheckResult(l_Device->CreateCommittedResource(&l_HeapProperties, D3D12_HEAP_FLAG_NONE, &l_BufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_TriangleVertices)), "CreateCommittedResource (forward triangle)"))
		{
			return false;
		}

		D3D12::SetDebugName(m_TriangleVertices.Get(), "Forward Triangle Vertices");

		const D3D12_RANGE l_NoRead = { 0, 0 };
		void* l_Mapped = nullptr;
		if (!D3D12::CheckResult(m_TriangleVertices->Map(0, &l_NoRead, &l_Mapped), "ID3D12Resource::Map (forward triangle)"))
		{
			return false;
		}

		std::memcpy(l_Mapped, l_Vertices.data(), l_Bytes);
		m_TriangleVertices->Unmap(0, nullptr);
		m_TriangleVertexCount = static_cast<uint32_t>(l_Vertices.size());

		// A raw view: the shader loads Vertex structs at byte offsets, so the view carries no stride of its own
		D3D12_SHADER_RESOURCE_VIEW_DESC l_View = {};
		l_View.Format = DXGI_FORMAT_R32_TYPELESS;
		l_View.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		l_View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		l_View.Buffer.FirstElement = 0;
		l_View.Buffer.NumElements = static_cast<UINT>(l_Bytes / sizeof(uint32_t));
		l_View.Buffer.StructureByteStride = 0;
		l_View.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

		// Persistent slot in the shader-visible heap; its index is what the draw constants carry
		m_TriangleView = m_ResourceHeap->Allocate();
		if (!m_TriangleView.IsValid())
		{
			return false;
		}

		l_Device->CreateShaderResourceView(m_TriangleVertices.Get(), &l_View, m_TriangleView.CPU);

		return true;
	}
}
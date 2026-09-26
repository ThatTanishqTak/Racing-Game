#include "Powertrain/RHI/D3D12/D3D12PipelineCache.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/RHI/D3D12/D3D12Device.hpp"

#include <format>
#include <fstream>
#include <iterator>

namespace Powertrain
{
	namespace
	{
		constexpr uint64_t k_FnvOffset = 14695981039346656037ull;
		constexpr uint64_t k_FnvPrime = 1099511628211ull;

		void HashBytes(uint64_t& hash, const void* data, size_t size)
		{
			const uint8_t* l_Bytes = static_cast<const uint8_t*>(data);
			for (size_t l_Index = 0; l_Index < size; ++l_Index)
			{
				hash ^= l_Bytes[l_Index];
				hash *= k_FnvPrime;
			}
		}

		template<typename T>
		void HashValue(uint64_t& hash, const T& value)
		{
			HashBytes(hash, &value, sizeof(T));
		}

		bool ReadBinaryFile(const std::filesystem::path& path, std::vector<uint8_t>& bytes)
		{
			std::ifstream l_File(path, std::ios::binary);
			if (!l_File)
			{
				return false;
			}

			bytes.assign(std::istreambuf_iterator<char>(l_File), std::istreambuf_iterator<char>());

			return !bytes.empty();
		}
	}

	size_t D3D12PipelineCache::DescriptionHash::operator()(const GraphicsPipelineDescription& description) const
	{
		// Field by field, never the whole struct, so padding bytes cannot make equal descriptions hash apart
		uint64_t l_Hash = k_FnvOffset;
		HashValue(l_Hash, description.VertexShader.size());
		HashBytes(l_Hash, description.VertexShader.data(), description.VertexShader.size());
		HashValue(l_Hash, description.PixelShader.size());
		HashBytes(l_Hash, description.PixelShader.data(), description.PixelShader.size());
		HashValue(l_Hash, description.RenderTargetFormats);
		HashValue(l_Hash, description.RenderTargetCount);
		HashValue(l_Hash, description.DepthFormat);
		HashValue(l_Hash, description.SampleCount);
		HashValue(l_Hash, description.Topology);
		HashValue(l_Hash, description.CullMode);
		HashValue(l_Hash, description.FillMode);
		HashValue(l_Hash, description.FrontCounterClockwise);
		HashValue(l_Hash, description.DepthTest);
		HashValue(l_Hash, description.DepthWrite);
		HashValue(l_Hash, description.DepthFunction);
		HashValue(l_Hash, description.Blend);

		return static_cast<size_t>(l_Hash);
	}

	D3D12PipelineCache::~D3D12PipelineCache()
	{
		Shutdown();
	}

	bool D3D12PipelineCache::Initialize(D3D12Device& device, const std::filesystem::path& shaderDirectory)
	{
		m_Device = device.GetHandle();
		m_ShaderDirectory = shaderDirectory;

		if (!CreateRootSignature())
		{
			Shutdown();

			return false;
		}

		m_Initialized = true;

		PT_CORE_INFO("Pipeline cache ready, shaders from '{}'", m_ShaderDirectory.string());

		return true;
	}

	void D3D12PipelineCache::Shutdown()
	{
		if (m_Initialized)
		{
			PT_CORE_INFO("Pipeline cache shut down with {} pipelines and {} shaders", m_Pipelines.size(), m_Shaders.size());
		}

		m_Pipelines.clear();
		m_Shaders.clear();
		m_RootSignature.Reset();
		m_ShaderDirectory.clear();
		m_Device = nullptr;
		m_Initialized = false;
	}

	ID3D12PipelineState* D3D12PipelineCache::GetGraphicsPipeline(const GraphicsPipelineDescription& description)
	{
		if (!m_Initialized)
		{
			return nullptr;
		}

		const auto l_Found = m_Pipelines.find(description);
		if (l_Found != m_Pipelines.end())
		{
			return l_Found->second.Get();
		}

		const std::vector<uint8_t>* l_VertexShader = GetShader(description.VertexShader);
		const std::vector<uint8_t>* l_PixelShader = description.PixelShader.empty() ? nullptr : GetShader(description.PixelShader);
		if (l_VertexShader == nullptr || (!description.PixelShader.empty() && l_PixelShader == nullptr))
		{
			return nullptr;
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC l_Pipeline = {};
		l_Pipeline.pRootSignature = m_RootSignature.Get();
		l_Pipeline.VS = { l_VertexShader->data(), l_VertexShader->size() };
		if (l_PixelShader != nullptr)
		{
			l_Pipeline.PS = { l_PixelShader->data(), l_PixelShader->size() };
		}

		// Straight alpha when blending; colours are linear and the sRGB target encodes on write
		D3D12_RENDER_TARGET_BLEND_DESC& l_Blend = l_Pipeline.BlendState.RenderTarget[0];
		l_Blend.BlendEnable = description.Blend == BlendMode::AlphaBlend ? TRUE : FALSE;
		l_Blend.LogicOpEnable = FALSE;
		l_Blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		l_Blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		l_Blend.BlendOp = D3D12_BLEND_OP_ADD;
		l_Blend.SrcBlendAlpha = D3D12_BLEND_ONE;
		l_Blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		l_Blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		l_Blend.LogicOp = D3D12_LOGIC_OP_NOOP;
		l_Blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		l_Pipeline.BlendState.AlphaToCoverageEnable = FALSE;
		l_Pipeline.BlendState.IndependentBlendEnable = FALSE;
		l_Pipeline.SampleMask = UINT_MAX;

		l_Pipeline.RasterizerState.FillMode = description.FillMode;
		l_Pipeline.RasterizerState.CullMode = description.CullMode;
		l_Pipeline.RasterizerState.FrontCounterClockwise = description.FrontCounterClockwise ? TRUE : FALSE;
		l_Pipeline.RasterizerState.DepthClipEnable = TRUE;
		l_Pipeline.RasterizerState.MultisampleEnable = description.SampleCount > 1 ? TRUE : FALSE;
		l_Pipeline.RasterizerState.AntialiasedLineEnable = FALSE;
		l_Pipeline.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		// Writing without testing still needs the depth stage on, so the comparison becomes always-pass
		l_Pipeline.DepthStencilState.DepthEnable = description.DepthTest || description.DepthWrite ? TRUE : FALSE;
		l_Pipeline.DepthStencilState.DepthWriteMask = description.DepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		l_Pipeline.DepthStencilState.DepthFunc = description.DepthTest ? description.DepthFunction : D3D12_COMPARISON_FUNC_ALWAYS;
		l_Pipeline.DepthStencilState.StencilEnable = FALSE;

		// Vertex pulling everywhere: no input layout on any pipeline
		l_Pipeline.InputLayout = { nullptr, 0 };
		l_Pipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		l_Pipeline.PrimitiveTopologyType = description.Topology;
		l_Pipeline.NumRenderTargets = description.RenderTargetCount;
		for (uint32_t l_Index = 0; l_Index < description.RenderTargetCount; ++l_Index)
		{
			l_Pipeline.RTVFormats[l_Index] = description.RenderTargetFormats[l_Index];
		}
		l_Pipeline.DSVFormat = description.DepthFormat;
		l_Pipeline.SampleDesc = { description.SampleCount, 0 };
		l_Pipeline.NodeMask = 0;

		const std::string l_Name = std::format("{} + {}", description.VertexShader, description.PixelShader.empty() ? "depth only" : description.PixelShader);

		ComPtr<ID3D12PipelineState> l_State;
		if (!D3D12::CheckResult(m_Device->CreateGraphicsPipelineState(&l_Pipeline, IID_PPV_ARGS(&l_State)), std::format("CreateGraphicsPipelineState ({})", l_Name)))
		{
			return nullptr;
		}

		D3D12::SetDebugName(l_State.Get(), l_Name);

		PT_CORE_INFO("Pipeline created: {}", l_Name);

		return m_Pipelines.emplace(description, std::move(l_State)).first->second.Get();
	}

	const std::vector<uint8_t>* D3D12PipelineCache::GetShader(std::string_view name)
	{
		const auto l_Found = m_Shaders.find(std::string(name));
		if (l_Found != m_Shaders.end())
		{
			return &l_Found->second;
		}

		std::vector<uint8_t> l_Bytes;
		if (!ReadBinaryFile(m_ShaderDirectory / std::format("{}.cso", name), l_Bytes))
		{
			PT_CORE_ERROR("Shader '{}' missing; the PowertrainShaders target writes it to '{}'", name, m_ShaderDirectory.string());

			return nullptr;
		}

		return &m_Shaders.emplace(std::string(name), std::move(l_Bytes)).first->second;
	}

	bool D3D12PipelineCache::CreateRootSignature()
	{
		// 0: 8 root constants per draw (b0), 1: per-frame CBV (b1), 2: per-pass CBV (b2). Everything else is reached through
		// ResourceDescriptorHeap[] and SamplerDescriptorHeap[], so there are no descriptor tables and one signature for every pass.
		D3D12_ROOT_PARAMETER1 l_Parameters[3] = {};
		l_Parameters[k_DrawConstantsParameter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		l_Parameters[k_DrawConstantsParameter].Constants.ShaderRegister = 0;
		l_Parameters[k_DrawConstantsParameter].Constants.RegisterSpace = 0;
		l_Parameters[k_DrawConstantsParameter].Constants.Num32BitValues = k_DrawConstantCount;
		l_Parameters[k_DrawConstantsParameter].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		// The parameter index doubles as the register number; the ring's contents are final before the list executes
		for (uint32_t l_Index = k_FrameConstantsParameter; l_Index <= k_PassConstantsParameter; ++l_Index)
		{
			l_Parameters[l_Index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			l_Parameters[l_Index].Descriptor.ShaderRegister = l_Index;
			l_Parameters[l_Index].Descriptor.RegisterSpace = 0;
			l_Parameters[l_Index].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
			l_Parameters[l_Index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		}

		// Version 1.1 has shipped with every Windows 10 build since 1607, and the Windows 11 floor decided at M6 makes it a given
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC l_Description = {};
		l_Description.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		l_Description.Desc_1_1.NumParameters = 3;
		l_Description.Desc_1_1.pParameters = l_Parameters;
		l_Description.Desc_1_1.NumStaticSamplers = 0;
		l_Description.Desc_1_1.pStaticSamplers = nullptr;
		l_Description.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		ComPtr<ID3DBlob> l_Serialized;
		ComPtr<ID3DBlob> l_Error;
		const HRESULT l_Result = D3D12SerializeVersionedRootSignature(&l_Description, &l_Serialized, &l_Error);
		if (FAILED(l_Result))
		{
			if (l_Error)
			{
				PT_CORE_ERROR("Bindless root signature: {}", static_cast<const char*>(l_Error->GetBufferPointer()));
			}

			return D3D12::CheckResult(l_Result, "D3D12SerializeVersionedRootSignature (bindless)");
		}

		if (!D3D12::CheckResult(m_Device->CreateRootSignature(0, l_Serialized->GetBufferPointer(), l_Serialized->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)), "CreateRootSignature (bindless)"))
		{
			return false;
		}

		D3D12::SetDebugName(m_RootSignature.Get(), "Bindless Root Signature");

		return true;
	}
}
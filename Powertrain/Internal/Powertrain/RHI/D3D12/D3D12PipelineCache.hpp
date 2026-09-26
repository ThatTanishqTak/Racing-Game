#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Powertrain
{
	class D3D12Device;

	enum class BlendMode : uint8_t
	{
		Opaque,
		AlphaBlend
	};

	struct GraphicsPipelineDescription
	{
		std::string VertexShader;

		std::string PixelShader;

		std::array<DXGI_FORMAT, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> RenderTargetFormats = {};
		uint32_t RenderTargetCount = 0;
		DXGI_FORMAT DepthFormat = DXGI_FORMAT_UNKNOWN;
		uint32_t SampleCount = 1;

		D3D12_PRIMITIVE_TOPOLOGY_TYPE Topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		D3D12_CULL_MODE CullMode = D3D12_CULL_MODE_BACK;
		D3D12_FILL_MODE FillMode = D3D12_FILL_MODE_SOLID;

		bool FrontCounterClockwise = true;

		int32_t DepthBias = 0;
		float SlopeScaledDepthBias = 0.0f;

		bool DepthTest = false;
		bool DepthWrite = false;

		D3D12_COMPARISON_FUNC DepthFunction = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

		BlendMode Blend = BlendMode::Opaque;

		bool operator==(const GraphicsPipelineDescription&) const = default;
	};

	class D3D12PipelineCache
	{
	public:
		static constexpr uint32_t k_DrawConstantsParameter = 0;
		static constexpr uint32_t k_FrameConstantsParameter = 1;
		static constexpr uint32_t k_PassConstantsParameter = 2;
		static constexpr uint32_t k_DrawConstantCount = 8;

		D3D12PipelineCache() = default;
		~D3D12PipelineCache();

		D3D12PipelineCache(const D3D12PipelineCache&) = delete;
		D3D12PipelineCache& operator=(const D3D12PipelineCache&) = delete;

		bool Initialize(D3D12Device& device, const std::filesystem::path& shaderDirectory);
		void Shutdown();

		ID3D12PipelineState* GetGraphicsPipeline(const GraphicsPipelineDescription& description);

		const std::vector<uint8_t>* GetShader(std::string_view name);

		ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }
		size_t GetPipelineCount() const { return m_Pipelines.size(); }

	private:
		struct DescriptionHash
		{
			size_t operator()(const GraphicsPipelineDescription& description) const;
		};

		bool CreateRootSignature();

	private:
		ID3D12Device* m_Device = nullptr;
		std::filesystem::path m_ShaderDirectory;

		ComPtr<ID3D12RootSignature> m_RootSignature;
		std::unordered_map<GraphicsPipelineDescription, ComPtr<ID3D12PipelineState>, DescriptionHash> m_Pipelines;
		std::unordered_map<std::string, std::vector<uint8_t>> m_Shaders;

		bool m_Initialized = false;
	};
}
#pragma once

#include "Powertrain/RHI/D3D12/D3D12Common.hpp"
#include "Powertrain/RHI/D3D12/D3D12DescriptorHeap.hpp"

#include <cstdint>
#include <vector>

namespace Powertrain
{
	class D3D12Renderer;
	class WindowsWindow;

	class ImGuiPass
	{
	public:
		ImGuiPass() = default;
		~ImGuiPass();

		ImGuiPass(const ImGuiPass&) = delete;
		ImGuiPass& operator=(const ImGuiPass&) = delete;

		bool Initialize(D3D12Renderer& renderer, WindowsWindow& window);
		void Shutdown();

		void BeginFrame();
		void EndFrame(ID3D12GraphicsCommandList* commandList);

		uint32_t GetDrawCallCount() const { return m_DrawCalls; }
		uint32_t GetTriangleCount() const { return m_Triangles; }

		void AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle);
		void FreeDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);

	private:
		D3D12DescriptorHeap* m_ResourceHeap = nullptr;

		std::vector<DescriptorHandle> m_Descriptors;

		uint32_t m_DrawCalls = 0;
		uint32_t m_Triangles = 0;

		bool m_Initialized = false;
		bool m_FrameBegun = false;
	};
}
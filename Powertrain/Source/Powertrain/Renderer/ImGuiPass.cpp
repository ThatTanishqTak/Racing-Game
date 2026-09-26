#include "Powertrain/Renderer/ImGuiPass.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Platform/Windows/WindowsWindow.hpp"
#include "Powertrain/RHI/D3D12/D3D12Renderer.hpp"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <cmath>
#include <cstddef>

// The Win32 backend implements this but leaves the declaration to the application
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

namespace Powertrain
{
	namespace
	{
		constexpr float k_DefaultDpi = 96.0f;

		// The swap chain RTV is sRGB, so the style colors are stored linear and the output encode restores the intended look
		void LinearizeStyle(ImGuiStyle& style)
		{
			for (ImVec4& l_Color : style.Colors)
			{
				l_Color.x = std::pow(l_Color.x, 2.2f);
				l_Color.y = std::pow(l_Color.y, 2.2f);
				l_Color.z = std::pow(l_Color.z, 2.2f);
			}
		}

		// Owns no state on purpose: DestroyWindow dispatches its last messages after the renderer, and this pass, are gone
		bool ForwardMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
		{
			if (ImGui::GetCurrentContext() == nullptr)
			{
				return false;
			}

			ImGui_ImplWin32_WndProcHandler(windowHandle, message, wParam, lParam);

			if (message == WM_DPICHANGED)
			{
				ImGui::GetStyle().FontScaleDpi = static_cast<float>(HIWORD(wParam)) / k_DefaultDpi;
			}

			// Never swallow anything: WindowsInput and DefWindowProc must still see every message
			return false;
		}

		void AllocateSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle)
		{
			static_cast<ImGuiPass*>(info->UserData)->AllocateDescriptor(*cpuHandle, *gpuHandle);
		}

		void FreeSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE)
		{
			static_cast<ImGuiPass*>(info->UserData)->FreeDescriptor(cpuHandle);
		}
	}

	ImGuiPass::~ImGuiPass()
	{
		Shutdown();
	}

	bool ImGuiPass::Initialize(D3D12Renderer& renderer, WindowsWindow& window)
	{
		m_ResourceHeap = &renderer.GetResourceHeap();
		const HWND l_WindowHandle = window.GetHandle();

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		// The window owns the cursor (lock and hide), so the backend never changes it
		ImGuiIO& l_IO = ImGui::GetIO();
		l_IO.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

		ImGui::StyleColorsDark();

		ImGuiStyle& l_Style = ImGui::GetStyle();
		const float l_DpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(l_WindowHandle);
		l_Style.ScaleAllSizes(l_DpiScale);
		l_Style.FontScaleDpi = l_DpiScale;
		LinearizeStyle(l_Style);

		if (!ImGui_ImplWin32_Init(l_WindowHandle))
		{
			PT_CORE_ERROR("ImGui_ImplWin32_Init failed");
			Shutdown();

			return false;
		}

		ImGui_ImplDX12_InitInfo l_Info = {};
		l_Info.Device = renderer.GetDevice().GetHandle();
		l_Info.CommandQueue = renderer.GetDirectQueue().GetHandle();
		l_Info.NumFramesInFlight = static_cast<int>(D3D12::k_FramesInFlight);
		l_Info.RTVFormat = D3D12SwapChain::k_RtvFormat;
		l_Info.DSVFormat = DXGI_FORMAT_UNKNOWN;
		l_Info.UserData = this;
		l_Info.SrvDescriptorHeap = m_ResourceHeap->GetHandle();
		l_Info.SrvDescriptorAllocFn = AllocateSrv;
		l_Info.SrvDescriptorFreeFn = FreeSrv;

		if (!ImGui_ImplDX12_Init(&l_Info))
		{
			PT_CORE_ERROR("ImGui_ImplDX12_Init failed");
			Shutdown();

			return false;
		}

		window.AddMessageHandler([l_WindowHandle](UINT message, WPARAM wParam, LPARAM lParam)
		{
			return ForwardMessage(l_WindowHandle, message, wParam, lParam);
		});

		m_Initialized = true;

		PT_CORE_INFO("ImGui {} initialized, DPI scale {:.2f}", IMGUI_VERSION, l_DpiScale);

		return true;
	}

	void ImGuiPass::Shutdown()
	{
		if (ImGui::GetCurrentContext() != nullptr)
		{
			if (m_FrameBegun)
			{
				ImGui::EndFrame();
				m_FrameBegun = false;
			}

			// The renderer backend goes first: it destroys its textures and hands their descriptors back through FreeSrv
			if (ImGui::GetIO().BackendRendererUserData != nullptr)
			{
				ImGui_ImplDX12_Shutdown();
			}

			if (ImGui::GetIO().BackendPlatformUserData != nullptr)
			{
				ImGui_ImplWin32_Shutdown();
			}

			ImGui::DestroyContext();
		}

		PT_CORE_ASSERT(m_Descriptors.empty(), "ImGui left {} descriptors allocated", m_Descriptors.size());
		if (m_ResourceHeap != nullptr)
		{
			for (DescriptorHandle& l_Handle : m_Descriptors)
			{
				m_ResourceHeap->Free(l_Handle);
			}
		}
		m_Descriptors.clear();

		m_ResourceHeap = nullptr;
		m_DrawCalls = 0;
		m_Triangles = 0;

		if (m_Initialized)
		{
			PT_CORE_INFO("ImGui shut down");
		}

		m_Initialized = false;
	}

	void ImGuiPass::BeginFrame()
	{
		PT_CORE_ASSERT(m_Initialized, "ImGuiPass::BeginFrame before Initialize");
		PT_CORE_ASSERT(!m_FrameBegun, "ImGuiPass::BeginFrame called twice without EndFrame");

		// The first call also builds the root signature, pipeline state and font texture
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		m_FrameBegun = true;
	}

	void ImGuiPass::EndFrame(ID3D12GraphicsCommandList* commandList)
	{
		if (!m_FrameBegun)
		{
			return;
		}

		m_FrameBegun = false;
		m_DrawCalls = 0;
		m_Triangles = 0;

		if (commandList == nullptr)
		{
			// The frame is abandoned; close ImGui's side so the next NewFrame passes its sanity check
			ImGui::EndFrame();

			return;
		}

		ImGui::Render();

		ImDrawData* l_DrawData = ImGui::GetDrawData();
		for (const ImDrawList* l_List : l_DrawData->CmdLists)
		{
			m_DrawCalls += static_cast<uint32_t>(l_List->CmdBuffer.Size);
		}
		m_Triangles = static_cast<uint32_t>(l_DrawData->TotalIdxCount / 3);

		ImGui_ImplDX12_RenderDrawData(l_DrawData, commandList);
	}

	void ImGuiPass::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle)
	{
		const DescriptorHandle l_Handle = m_ResourceHeap->Allocate();
		PT_CORE_ASSERT(l_Handle.IsValid(), "Resource heap has no persistent slot left for ImGui");

		m_Descriptors.push_back(l_Handle);

		cpuHandle = l_Handle.CPU;
		gpuHandle = l_Handle.GPU;
	}

	void ImGuiPass::FreeDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle)
	{
		for (size_t l_Index = 0; l_Index < m_Descriptors.size(); ++l_Index)
		{
			if (m_Descriptors[l_Index].CPU.ptr == cpuHandle.ptr)
			{
				m_ResourceHeap->Free(m_Descriptors[l_Index]);
				m_Descriptors.erase(m_Descriptors.begin() + static_cast<std::ptrdiff_t>(l_Index));

				return;
			}
		}

		PT_CORE_WARN("ImGui freed a descriptor this pass did not allocate");
	}
}
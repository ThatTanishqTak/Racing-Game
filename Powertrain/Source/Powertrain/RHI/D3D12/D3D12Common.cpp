#include "Powertrain/RHI/D3D12/D3D12Common.hpp"

#include "Powertrain/Core/CoreLog.hpp"

#include <format>

namespace Powertrain
{
	namespace D3D12
	{
		std::string ResultToString(HRESULT result)
		{
			std::string l_Message = std::format("0x{:08X}", static_cast<uint32_t>(result));

			char* l_Buffer = nullptr;
			const DWORD l_Length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(result), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&l_Buffer), 0, nullptr);
			if (l_Length != 0 && l_Buffer != nullptr)
			{
				std::string_view l_Text(l_Buffer, l_Length);
				while (!l_Text.empty() && (l_Text.back() == '\r' || l_Text.back() == '\n' || l_Text.back() == ' ' || l_Text.back() == '.'))
				{
					l_Text.remove_suffix(1);
				}

				l_Message += ": ";
				l_Message += l_Text;
			}

			if (l_Buffer != nullptr)
			{
				LocalFree(l_Buffer);
			}

			return l_Message;
		}

		bool CheckResult(HRESULT result, std::string_view what)
		{
			if (FAILED(result))
			{
				PT_CORE_ERROR("{} failed: {}", what, ResultToString(result));

				return false;
			}

			return true;
		}

		void SetDebugName(ID3D12Object* object, std::string_view name)
		{
#ifdef PT_DEBUG
			if (object != nullptr)
			{
				object->SetName(Win32::ToWide(name).c_str());
			}
#else
			(void)object;
			(void)name;
#endif
		}
	}
}
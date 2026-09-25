#include "Powertrain/Platform/Windows/Win32.hpp"

namespace Powertrain
{
	namespace Win32
	{
		std::wstring ToWide(std::string_view text)
		{
			if (text.empty())
			{
				return std::wstring();
			}

			const int l_Length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
			std::wstring l_Result(static_cast<size_t>(l_Length), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), l_Result.data(), l_Length);

			return l_Result;
		}

		std::string ToNarrow(std::wstring_view text)
		{
			if (text.empty())
			{
				return std::string();
			}

			const int l_Length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
			std::string l_Result(static_cast<size_t>(l_Length), '\0');
			WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), l_Result.data(), l_Length, nullptr, nullptr);

			return l_Result;
		}
	}
}
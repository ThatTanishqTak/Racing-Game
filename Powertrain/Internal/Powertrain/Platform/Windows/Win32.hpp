#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace Powertrain
{
	namespace Win32
	{
		std::wstring ToWide(std::string_view text);
		std::string ToNarrow(std::wstring_view text);

		// Folder of the running executable
		std::filesystem::path GetExecutableDirectory();
	}
}
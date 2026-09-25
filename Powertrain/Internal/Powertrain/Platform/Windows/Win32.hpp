#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <string>
#include <string_view>

namespace Powertrain
{
	namespace Win32
	{
		std::wstring ToWide(std::string_view text);
	}
}
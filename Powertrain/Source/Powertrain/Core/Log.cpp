#include "Powertrain/Core/Log.hpp"

#include <Windows.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace Powertrain
{
	namespace
	{
		std::string_view GetSourceName(LogSource source)
		{
			switch (source)
			{
				case LogSource::Core:
					return "Core";
				case LogSource::Client:
					return "App";
			}

			return "?";
		}

		std::string_view GetLevelName(LogLevel level)
		{
			switch (level)
			{
				case LogLevel::Trace:
				{
					return "TRACE";
				}
				case LogLevel::Info:
				{
					return "INFO ";
				}
				case LogLevel::Warn:
				{
					return "WARN ";
				}
				case LogLevel::Error:
				{
					return "ERROR";
				}
				case LogLevel::Fatal:
				{
					return "FATAL";
				}
			}

			return "?!?!?!?!";
		}

		const char* GetLevelColor(LogLevel level)
		{
			switch (level)
			{
				case LogLevel::Trace:
				{
					return "\x1b[90m"; // Gray
				}
				case LogLevel::Info:
				{
					return "\x1b[32m"; // Green
				}
				case LogLevel::Warn:
				{
					return "\x1b[33m"; // Yellow

				}case LogLevel::Error:
				{
					return "\x1b[31m"; // Red
				}
				case LogLevel::Fatal:
				{
					return "\x1b[97;41m"; // White on red bg
				}
			}

			return "\x1b[0m"; // Default
		}

		std::wstring ToWide(std::string_view text)
		{
			const int l_Length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
			std::wstring l_Result(static_cast<size_t>(l_Length), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), l_Result.data(), l_Length);

			return l_Result;
		}

		std::filesystem::path GetExecutableDirectory()
		{
			std::wstring l_Buffer(MAX_PATH, L'\0');
			DWORD l_Length = GetModuleFileNameW(nullptr, l_Buffer.data(), static_cast<DWORD>(l_Buffer.size()));
			while (l_Length == l_Buffer.size())
			{
				l_Buffer.resize(l_Buffer.size() * 2);
				l_Length = GetModuleFileNameW(nullptr, l_Buffer.data(), static_cast<DWORD>(l_Buffer.size()));
			}

			l_Buffer.resize(l_Length);

			return std::filesystem::path(l_Buffer).parent_path();
		}

		class LogSink
		{
		public:
			LogSink() : m_StartTime(std::chrono::steady_clock::now())
			{
				const std::filesystem::path l_LogDirectory = GetExecutableDirectory() / "Logs";

				std::error_code l_Error;
				std::filesystem::create_directories(l_LogDirectory, l_Error);
				m_File.open(l_LogDirectory / "Powertrain.log", std::ios::out | std::ios::trunc);

#ifdef PT_DEBUG
				HANDLE l_Console = GetStdHandle(STD_OUTPUT_HANDLE);
				DWORD l_Mode = 0;
				if (l_Console != INVALID_HANDLE_VALUE && l_Console != nullptr && GetConsoleMode(l_Console, &l_Mode))
				{
					SetConsoleMode(l_Console, l_Mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
					m_ConsoleEnabled = true;
				}
#endif
			}

			void Write(LogSource source, LogLevel level, std::string_view message)
			{
				const double l_Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_StartTime).count();
				const std::string l_Line = std::format("[{:10.3f}] [{}] [{}] {}\n", l_Seconds, GetSourceName(source), GetLevelName(level), message);

				std::scoped_lock l_Lock(m_Mutex);

				if (m_File.is_open())
				{
					m_File << l_Line;
					if (level >= LogLevel::Error)
					{
						m_File.flush();
					}
				}

				if (m_ConsoleEnabled)
				{
					std::fputs(GetLevelColor(level), stdout);
					std::fwrite(l_Line.data(), 1, l_Line.size(), stdout);
					std::fputs("\x1b[0m", stdout);
				}

				OutputDebugStringW(ToWide(l_Line).c_str());
			}

		private:
			std::chrono::steady_clock::time_point m_StartTime;
			std::ofstream m_File;
			std::mutex m_Mutex;

			bool m_ConsoleEnabled = false;
		};

		LogSink g_LogSink;
	}

	void WriteLog(LogSource source, LogLevel level, std::string_view message)
	{
		g_LogSink.Write(source, level, message);
	}
}
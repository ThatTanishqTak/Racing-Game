#pragma once

#include <cstdint>
#include <format>
#include <string_view>

namespace Powertrain
{
	enum class LogSource : uint8_t
	{
		Core,
		Client
	};

	enum class LogLevel : uint8_t
	{
		Trace,
		Info,
		Warn,
		Error,
		Fatal
	};

	void WriteLog(LogSource source, LogLevel level, std::string_view message);
}

#define PT_TRACE(...) ::Powertrain::WriteLog(::Powertrain::LogSource::Client, ::Powertrain::LogLevel::Trace, ::std::format(__VA_ARGS__))
#define PT_INFO(...) ::Powertrain::WriteLog(::Powertrain::LogSource::Client, ::Powertrain::LogLevel::Info, ::std::format(__VA_ARGS__))
#define PT_WARN(...) ::Powertrain::WriteLog(::Powertrain::LogSource::Client, ::Powertrain::LogLevel::Warn, ::std::format(__VA_ARGS__))
#define PT_ERROR(...) ::Powertrain::WriteLog(::Powertrain::LogSource::Client, ::Powertrain::LogLevel::Error, ::std::format(__VA_ARGS__))
#define PT_FATAL(...) ::Powertrain::WriteLog(::Powertrain::LogSource::Client, ::Powertrain::LogLevel::Fatal, ::std::format(__VA_ARGS__))

#ifdef PT_DEBUG
#define PT_ASSERT(condition, ...) do { if (!(condition)) { ::Powertrain::WriteLog(::Powertrain::LogSource::Client, ::Powertrain::LogLevel::Fatal, ::std::format("Assertion '{}' failed at {}:{}: {}", #condition, __FILE__, __LINE__, ::std::format(__VA_ARGS__))); __debugbreak(); } } while (false)
#else
#define PT_ASSERT(condition, ...) do { (void)sizeof(condition); } while (false)
#endif
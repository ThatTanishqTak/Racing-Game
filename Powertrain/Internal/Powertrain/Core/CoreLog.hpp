#pragma once

#include "Powertrain/Core/Log.hpp"

#define PT_CORE_TRACE(...) ::Powertrain::WriteLog(::Powertrain::LogSource::Core, ::Powertrain::LogLevel::Trace, ::std::format(__VA_ARGS__))
#define PT_CORE_INFO(...) ::Powertrain::WriteLog(::Powertrain::LogSource::Core, ::Powertrain::LogLevel::Info, ::std::format(__VA_ARGS__))
#define PT_CORE_WARN(...) ::Powertrain::WriteLog(::Powertrain::LogSource::Core, ::Powertrain::LogLevel::Warn, ::std::format(__VA_ARGS__))
#define PT_CORE_ERROR(...) ::Powertrain::WriteLog(::Powertrain::LogSource::Core, ::Powertrain::LogLevel::Error, ::std::format(__VA_ARGS__))
#define PT_CORE_FATAL(...) ::Powertrain::WriteLog(::Powertrain::LogSource::Core, ::Powertrain::LogLevel::Fatal, ::std::format(__VA_ARGS__))

#ifdef PT_DEBUG
#define PT_CORE_ASSERT(condition, ...) do { if (!(condition)) { ::Powertrain::WriteLog(::Powertrain::LogSource::Core, ::Powertrain::LogLevel::Fatal, ::std::format("Assertion '{}' failed at {}:{}: {}", #condition, __FILE__, __LINE__, ::std::format(__VA_ARGS__))); __debugbreak(); } } while (false)
#else
#define PT_CORE_ASSERT(condition, ...) do { (void)sizeof(condition); } while (false)
#endif
#pragma once

#include <cstdint>
#include <string>

namespace Powertrain
{
	struct WindowProperties
	{
		std::string Title = "Powertrain-Window";

		uint32_t WindowWidth = 1080;
		uint32_t WindowHeight = 720;
	};

	class Window
	{
	public:
		virtual ~Window() = default;

		virtual bool OnInitialize(const WindowProperties& properties = WindowProperties()) = 0;
		virtual void OnShutdown() = 0;
		virtual void OnUpdate() = 0;

		virtual bool ShouldClose() const = 0;
		virtual void* GetNativeWindow() const = 0;
	};
}
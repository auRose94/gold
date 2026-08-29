#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "types.hpp"

namespace gold {
	/** Opaque platform window handles handed to the renderer. */
	struct nativeWindow {
		void* handle = nullptr;   // X11 Window / HWND / NSWindow / wl_surface
		void* display = nullptr;  // X11 Display / EGLDisplay / wl_display
		void* context = nullptr;  // GL context / EGL context
	};

	/** Backend-agnostic window/input event produced by a windowSystem. */
	struct windowEvent {
		enum class eventType {
			NoneEvent,
			Quit,
			Resized,
			Moved,
			Shown,
			Hidden,
			Minimized,
			Maximized,
			Restored,
			FocusGained,
			FocusLost,
			KeyDown,
			KeyUp,
			TextInput,
			MouseDown,
			MouseUp,
			MouseMove,
			MouseWheel,
		} type = eventType::NoneEvent;

		int32_t x = 0;
		int32_t y = 0;
		int32_t width = 0;
		int32_t height = 0;
		int32_t keyCode = 0;
		int32_t button = 0;
		int32_t scrollX = 0;
		int32_t scrollY = 0;
		string text;
	};

	/** Convert a backend windowEvent into a gold object event:
	 * {"type","resized","width",800,"height",600} etc. */
	object eventToObject(const windowEvent& ev);

	/**
	 * Abstract window system backend. Implementations wrap a platform window
	 * toolkit (SDL, X11, Wayland, Win32, Cocoa, headless, ...). The gold
	 * `window` object is a facade over one of these; no backend types leak
	 * into the public API.
	 *
	 * Lifecycle: the engine creates a windowSystem, then the window object
	 * drives it via create()/pump()/destroy(). `native()` hands the platform
	 * window to the render backend.
	 */
	class windowSystem {
	 public:
		virtual ~windowSystem() = default;

		/** Create the native window from an object config. */
		virtual bool create(object config) = 0;
		/** Destroy the native window. */
		virtual void destroy() = 0;

		/** Pump one window event as a gold object. Returns false when no
		 * events remain. */
		virtual bool poll(object& out) = 0;

		/** Platform window handles for the render backend. */
		virtual nativeWindow native() const = 0;

		virtual void setTitle(const string& title) = 0;
		virtual void setSize(int32_t width, int32_t height) = 0;
		virtual void setPos(int32_t x, int32_t y) = 0;
		virtual void setFullscreen(bool fullscreen, bool desktop) = 0;
		virtual void setBorderless(bool borderless) = 0;

		/** Backend name, e.g. "sdl", "x11", "headless". */
		virtual const char* name() const = 0;
	};

	using createWindowSystemFn = windowSystem* (*)();
	using applicationDataDirFn = string (*)(const string& company,
																					const string& gameName);

	/**
	 * Create a window system backend by name ("sdl", "headless", ...).
	 * Backends may be statically linked (built into the game module) or
	 * loaded at runtime through the module loader. Returns nullptr if the
	 * backend is unavailable.
	 */
	windowSystem* createWindowSystem(const string& name);

	/** Create a window system, trying each backend name in order and
	 * returning the first one that can be created. */
	windowSystem* createWindowSystem(const list& names);

	/** Register a backend factory so createWindowSystem can find it. */
	void registerWindowSystem(const string& name, createWindowSystemFn fn);

	/** Register a provider for per-user app data directories. */
	void registerApplicationDataDir(applicationDataDirFn fn);

	/** Path to per-user application data (settings, saves). */
	string applicationDataDir(const string& company, const string& gameName);
}  // namespace gold
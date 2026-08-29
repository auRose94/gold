#pragma once

#include <functional>
#include <string>

namespace gold {
	/**
	 * Runtime loader for optional gold modules (game, web, ...).
	 *
	 * The game and web subsystems are built as shared libraries
	 * (libgoldGame.so, libgoldWeb.so). If your app only needs the core, you
	 * can link nothing extra; at runtime, call module::load("game") before
	 * constructing game objects to pull in the shared library. This makes
	 * the optional subsystems load-on-demand instead of being statically
	 * baked into every binary.
	 */
	struct module {
		/** Load a module by name (e.g. "game", "web"). Returns true if the
		 * module is available (already loaded, or found and loaded now). */
		static bool load(const std::string& name);
		/** Whether the named module is already loaded. */
		static bool isLoaded(const std::string& name);
		/** Path used to look up module libraries, e.g. the directory
		 * containing the executable. Defaults to "." + "/libgold<name>.so". */
		static void setLibraryPath(const std::string& path);
	};
}  // namespace gold
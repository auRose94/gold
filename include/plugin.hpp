#pragma once

#include <string>
#include <vector>

namespace gold {
	/**
	 * Runtime plugin loader.
	 *
	 * gold stays light and flexible: optional subsystems and platform
	 * backends live in shared libraries, loaded on demand instead of
	 * statically baked into every binary. Two kinds of loads share this
	 * API:
	 *
	 * - **Gold plugins** — a plugin is gold's own code built as a shared
	 *   library (e.g. libgoldSdl3.so, libgoldBgfx.so) compiled against the
	 *   system package's headers it wraps. Loading one is enough: its
	 *   static initializers call registerWindowSystem /
	 *   registerInputSystem / ... and the subsystems' `createX(name)`
	 *   registries see the new backends. The createX chains call
	 *   `plugin::load` automatically when a name misses. Policy for name
	 *   resolution: `libgold` + capitalized name + platform suffix.
	 * - **System C libraries** — dependencies with a stable C ABI
	 *   (FreeType today) are opened directly with typed function pointers
	 *   (`openSystemLibrary` + `symbol`); a machine without one degrades
	 *   gracefully to the fallback backend instead of failing to start.
	 *
	 * Dependency policy: system package -> platform parity package ->
	 * feature absent. The `sonames()` table encodes the versioned probes
	 * for the known dependencies; platforms that package different
	 * equivalents get parity entries there in later phases.
	 */
	struct plugin {
		/** Try to load a gold plugin by backend name; e.g. "sdl3" looks
		 *  for libgoldSdl3.so through the search paths. On success the
		 *  plugin's registrars have run and `createX(name)` finds it.
		 *  Returns false (with `lastError() explaining why) when absent. */
		static bool load(const std::string& name);

		/** Whether the named gold plugin is already loaded. */
		static bool isLoaded(const std::string& name);

		/** Diagnostics of the most recent failed load/open. */
		static std::string lastError();

		/** Directories searched for plugins and system libraries, in
		 *  order: $GOLD_PLUGIN_PATH (colon-separated), the executable's
		 *  directory, the directories of registered host modules (the
		 *  modules a binary links — plugins ship next to them), then
		 *  the module loader's configured library path. */
		static std::vector<std::string> searchPaths();

		/**
		 * Anchor a search directory to the module containing `address`
		 * (a function or data symbol inside a gold module, e.g. the
		 * calling registry's own createX function). Resolved via
		 * dladdr/GetModuleHandleEx once; repeat calls are cheap no-ops.
		 * This is how a linked game module (wherever it lives) points
		 * the loader at its plugin siblings.
		 */
		static void addModulePath(void* address);

		/** The plugin names to try for a config-style backend name, in
		 *  preference order. Generic aliases ("sdl") expand to the
		 *  concrete candidates ("sdl3" then the queued "sdl2"); known
		 *  names map to themselves. */
		static std::vector<std::string> pluginCandidates(
			const std::string& name);

		// ------------------------------------------------------------ system
		/**
		 * Open a third-party C library directly. Probes each candidate
		 * soname in order and returns the first handle that opens
		 * (nullptr when none). Candidates are full sonames as resolved by
		 * the system loader, e.g. "libSDL3.so.0" — typically taken from
		 * `sonames()`.
		 */
		static void* openSystemLibrary(
			const std::vector<std::string>& sonames);
		/** Look a symbol up in an opened system library (typed by the
		 *  caller). */
		static void* symbol(void* library, const std::string& name);
		/** Close an opened system library. The caller must know no live
		 *  pointers into it remain. */
		static void closeLibrary(void* library);

		/**
		 * Known versioned sonames to probe for `name`, e.g. "freetype" ->
		 * libfreetype.so.6. Prefer `probe` first: it never leaves the
		 * library loaded (system C libraries have no gold registrars).
		 */
		static std::vector<std::string> sonames(const std::string& name);

		/** True when one of `sonames(name)` can be opened (and is
		 *  immediately closed). Never call this for gold plugins: loading
		 *  those runs their registrars, which must not be undone. */
		static bool probe(const std::string& name);
	};

}  // namespace gold
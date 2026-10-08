#include "plugin.hpp"

#include <cstring>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace gold {

	namespace {
		std::mutex& mtx() {
			static std::mutex m;
			return m;
		}

		// Gold plugins stay loaded for the process lifetime: dlclose would
		// make the registrar-written factory pointers dangle. System C
		// libraries may be closed explicitly (closeLibrary).
		std::map<std::string, void*>& loaded() {
			static std::map<std::string, void*> libs;
			return libs;
		}

		std::string& lastErrorStorage() {
			static std::string error;
			return error;
		}

		void recordError(const std::string& message) {
			std::lock_guard<std::mutex> guard(mtx());
			lastErrorStorage() = message;
		}

		/** For functions that already hold mtx(). */
		void setHeldError(const std::string& message) {
			lastErrorStorage() = message;
		}

#ifdef _WIN32
		std::string executableDir() {
			char path[MAX_PATH];
			GetModuleFileNameA(nullptr, path, MAX_PATH);
			std::string dir = path;
			auto slash = dir.find_last_of("\\/");
			return slash == std::string::npos ? "." : dir.substr(0, slash);
		}
#else
		std::string executableDir() {
			char buffer[4096];
			ssize_t size = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
#if defined(__APPLE__)
			if (size <= 0) {
				uint32_t len = sizeof(buffer);
				if (_NSGetExecutablePath(buffer, &len) == 0)
					size = (ssize_t)strlen(buffer);
			}
#endif
			if (size <= 0) return ".";
			buffer[size] = '\0';
			std::string dir = buffer;
			auto slash = dir.find_last_of("/");
			return slash == std::string::npos ? "." : dir.substr(0, slash);
		}
#endif

		/** "sdl3" -> "libgoldSdl3.so" (name convention for gold plugins). */
		std::string pluginFileName(const std::string& name) {
			std::string lib = "libgold" + name;
			if (lib.size() > 7) lib[7] = char(std::toupper((unsigned char)lib[7]));
#ifdef _WIN32
			return lib + ".dll";
#elif defined(__APPLE__)
			return lib + ".dylib";
#else
			return lib + ".so";
#endif
		}
	}  // namespace

	std::vector<std::string> plugin::searchPaths() {
		std::vector<std::string> out;
		char* env = getenv("GOLD_PLUGIN_PATH");
		if (env && *env) {
			std::stringstream stream(env);
			std::string item;
			while (std::getline(stream, item, ':'))
				if (!item.empty()) out.push_back(item);
		}
		out.push_back(executableDir());
		return out;
	}

	std::string plugin::lastError() {
		std::lock_guard<std::mutex> guard(mtx());
		return lastErrorStorage();
	}

	bool plugin::isLoaded(const std::string& name) {
		std::lock_guard<std::mutex> guard(mtx());
		return loaded().count(name) > 0;
	}

	bool plugin::load(const std::string& name) {
		std::lock_guard<std::mutex> guard(mtx());
		if (loaded().count(name) > 0) return true;
		const std::string file = pluginFileName(name);
		// Search each directory; remember the first loader diagnostic that
		// looked plausible so an absent file explains itself.
		std::string diagnostics;
		for (const auto& dir : searchPaths()) {
#ifdef _WIN32
			void* handle = LoadLibraryA((dir + "\\" + file).c_str());
#else
			void* handle = dlopen((dir + "/" + file).c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
			if (handle) {
				loaded()[name] = handle;
				lastErrorStorage().clear();
				return true;
			}
#ifdef _WIN32
			// LoadLibraryA diagnostics come through GetLastError; keep the
			// generic message text.
			diagnostics = (dir + "\\" + file + ": LoadLibrary failed");
#else
			const char* err = dlerror();
			if (err && diagnostics.empty()) diagnostics = err;
#endif
		}
		if (diagnostics.empty())
			setHeldError(file + " not found in " +
						 std::to_string(searchPaths().size()) +
						 " search path(s)");
		else
			setHeldError(diagnostics);
		return false;
	}

	void* plugin::openSystemLibrary(const std::vector<std::string>& sonames) {
		std::string diagnostics;
		for (const auto& soname : sonames) {
#ifdef _WIN32
			void* handle = LoadLibraryA(soname.c_str());
#else
			void* handle = dlopen(soname.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
			if (handle) {
				std::lock_guard<std::mutex> guard(mtx());
				lastErrorStorage().clear();
				return handle;
			}
#ifdef _WIN32
			diagnostics = soname + ": LoadLibrary failed";
#else
			const char* err = dlerror();
			if (err && diagnostics.empty()) diagnostics = err;
#endif
		}
		std::lock_guard<std::mutex> guard(mtx());
		if (diagnostics.empty())
			lastErrorStorage() = "none of the candidate libraries could be opened";
		else
			lastErrorStorage() = diagnostics;
		return nullptr;
	}

	void* plugin::symbol(void* library, const std::string& name) {
		if (!library) return nullptr;
#ifdef _WIN32
		if (name.empty()) return nullptr;
		void* sym = (void*)GetProcAddress((HMODULE)library, name.c_str());
#else
		dlerror();
		void* sym = dlsym(library, name.c_str());
		const char* err = dlerror();
		if (err) sym = nullptr;
#endif
		if (!sym)
			recordError("symbol not found: " + name);
		return sym;
	}

	void plugin::closeLibrary(void* library) {
		if (!library) return;
#ifdef _WIN32
		FreeLibrary((HMODULE)library);
#else
		dlclose(library);
#endif
	}

	std::vector<std::string> plugin::sonames(const std::string& name) {
		// The version numbers here are ABI probes, not exact pins: the
		// loader takes the first that opens. Parity candidates (a
		// different-but-equivalent library this platform packages instead)
		// belong in the same list, most preferred first.
		if (name == "freetype")
			return {"libfreetype.so.6", "libfreetype.so", "freetype.dll"};
		if (name == "sdl3")
			return {"libSDL3.so.0", "libSDL3.so", "SDL3.dll"};
		if (name == "sdl2")
			return {"libSDL2-2.0.so.0", "libSDL2.so", "SDL2.dll"};
		if (name == "websockets")
			return {"libwebsockets.so.19", "libwebsockets.so.18",
				"libwebsockets.so", "libwebsockets.dll"};
		if (name == "bullet")
			return {"libbullet.so.3.27", "libbullet.so", "bullet.dll"};
		if (name == "zlib")
			return {"libz.so.1", "libz.so", "zlib1.dll"};
		return {};
	}

	bool plugin::probe(const std::string& name) {
		auto candidates = sonames(name);
		for (const auto& soname : candidates) {
#ifdef _WIN32
			void* handle = LoadLibraryA(soname.c_str());
			if (handle) {
				FreeLibrary((HMODULE)handle);
				return true;
			}
#else
			// Local + lazy: a probe must not run registrars or resolve
			// symbols.
			void* handle = dlopen(soname.c_str(), RTLD_LAZY | RTLD_LOCAL);
			if (handle) {
				dlclose(handle);
				return true;
			}
#endif
		}
		if (!candidates.empty())
			recordError("probe failed for " + name);
		return false;
	}

}  // namespace gold
#include "module.hpp"

#include <map>
#include <mutex>
#include <cctype>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace gold {

	namespace {
		std::string& libraryPath() {
			static std::string path = ".";
			return path;
		}
		std::map<std::string, void*>& loaded() {
			static std::map<std::string, void*> libs;
			return libs;
		}
		std::mutex& mtx() {
			static std::mutex m;
			return m;
		}
		std::string libName(const std::string& name) {
			// CMake target names are capitalized (goldWeb, goldGame), so map
			// "web" -> "Web" / "game" -> "Game" for a friendly API.
			std::string lib = "gold" + name;
			if (!lib.empty()) lib[4] = char(std::toupper((unsigned char)lib[4]));
			lib = "lib" + lib;
#ifdef _WIN32
			return libraryPath() + "\\" + lib + ".dll";
#elif defined(__APPLE__)
			return libraryPath() + "/" + lib + ".dylib";
#else
			return libraryPath() + "/" + lib + ".so";
#endif
		}
	}  // namespace

	void module::setLibraryPath(const std::string& path) {
		std::lock_guard<std::mutex> guard(mtx());
		libraryPath() = path;
	}

	bool module::isLoaded(const std::string& name) {
		std::lock_guard<std::mutex> guard(mtx());
		return loaded().count(name) > 0;
	}

	bool module::load(const std::string& name) {
		std::lock_guard<std::mutex> guard(mtx());
		if (loaded().count(name) > 0) return true;
#ifdef _WIN32
		auto handle = LoadLibraryA(libName(name).c_str());
#else
		auto handle = dlopen(libName(name).c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
		if (handle) {
			loaded()[name] = handle;
			return true;
		}
		return false;
	}

}  // namespace gold
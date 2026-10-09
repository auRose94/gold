#include "game/renderBackend.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <utility>

#include "plugin.hpp"

namespace gold {

	namespace {

		std::mutex& registryMutex() {
			static std::mutex m;
			return m;
		}
		std::map<renderBackendType, renderBackend* (*)()>& factories() {
			static std::map<renderBackendType, renderBackend* (*)()> f;
			return f;
		}
		std::map<std::string, renderBackend* (*)()>& namedFactories() {
			static std::map<std::string, renderBackend* (*)()> f;
			return f;
		}
	}  // namespace

	void registerRenderBackend(renderBackendType type,
		renderBackend* (*factory)()) {
		std::lock_guard<std::mutex> guard(registryMutex());
		factories()[type] = factory;
	}

	renderBackend* createRenderBackend(renderBackendType type) {
		std::lock_guard<std::mutex> guard(registryMutex());
		auto it = factories().find(type);
		if (it != factories().end()) return it->second();
		return nullptr;
	}

	void registerRenderBackendName(const std::string& name,
		renderBackend* (*factory)()) {
		std::lock_guard<std::mutex> guard(registryMutex());
		namedFactories()[name] = factory;
	}

	renderBackend* createRenderBackend(const std::string& name) {
		plugin::addModulePath((void*)registerRenderBackendName);
		{
			std::lock_guard<std::mutex> guard(registryMutex());
			auto it = namedFactories().find(name);
			if (it != namedFactories().end()) return it->second();
		}
		// Miss: a render plugin may provide it (self-registers on load);
		// try each candidate. Loading runs outside the registry mutex.
		for (const auto& candidate : plugin::pluginCandidates(name)) {
			if (!plugin::load(candidate)) continue;
			std::lock_guard<std::mutex> guard(registryMutex());
			auto it = namedFactories().find(name);
			if (it != namedFactories().end()) return it->second();
		}
		return nullptr;
	}

}  // namespace gold
#include "game/inputSystem.hpp"

#include <map>
#include <mutex>

#include "plugin.hpp"

namespace gold {

	namespace {
		std::mutex& registryMutex() {
			static std::mutex m;
			return m;
		}
		std::map<std::string, inputSystem* (*)()>& factories() {
			static std::map<std::string, inputSystem* (*)()> f;
			return f;
		}
	}  // namespace

	void registerInputSystem(const std::string& name,
		inputSystem* (*factory)()) {
		std::lock_guard<std::mutex> guard(registryMutex());
		factories()[name] = factory;
	}

	inputSystem* createInputSystem(const std::string& name) {
		{
			std::lock_guard<std::mutex> guard(registryMutex());
			auto it = factories().find(name);
			if (it != factories().end()) return it->second();
		}
		// Miss: a backend plugin may provide it (self-registers on load);
		// try each candidate. Loading runs outside the registry mutex.
		for (const auto& candidate : plugin::pluginCandidates(name)) {
			if (!plugin::load(candidate)) continue;
			std::lock_guard<std::mutex> guard(registryMutex());
			auto it = factories().find(name);
			if (it != factories().end()) return it->second();
		}
		return nullptr;
	}

}  // namespace gold
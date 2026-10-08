#include "game/audioSystem.hpp"

#include <map>
#include <mutex>

#include "plugin.hpp"

namespace gold {

	namespace {
		std::mutex& audioMutex() {
			static std::mutex m;
			return m;
		}
		std::map<std::string, audioSystem* (*)()>& factories() {
			static std::map<std::string, audioSystem* (*)()> f;
			return f;
		}
	}  // namespace

	void registerAudioSystem(const std::string& name,
		audioSystem* (*factory)()) {
		std::lock_guard<std::mutex> guard(audioMutex());
		factories()[name] = factory;
	}

	audioSystem* createAudioSystem(const std::string& name) {
		plugin::addModulePath((void*)registerAudioSystem);
		{
			std::lock_guard<std::mutex> guard(audioMutex());
			auto it = factories().find(name);
			if (it != factories().end()) return it->second();
		}
		// Miss: a backend plugin may provide it (self-registers on load);
		// try each candidate. Loading runs outside the registry mutex.
		for (const auto& candidate : plugin::pluginCandidates(name)) {
			if (!plugin::load(candidate)) continue;
			std::lock_guard<std::mutex> guard(audioMutex());
			auto it = factories().find(name);
			if (it != factories().end()) return it->second();
		}
		return nullptr;
	}

}  // namespace gold
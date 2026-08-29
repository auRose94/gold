#include "game/audioSystem.hpp"

#include <map>
#include <mutex>

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
		std::lock_guard<std::mutex> guard(audioMutex());
		auto it = factories().find(name);
		if (it != factories().end()) return it->second();
		return nullptr;
	}

}  // namespace gold
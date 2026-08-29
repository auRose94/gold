#include "game/inputSystem.hpp"

#include <map>
#include <mutex>

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
		std::lock_guard<std::mutex> guard(registryMutex());
		auto it = factories().find(name);
		if (it != factories().end()) return it->second();
		return nullptr;
	}

}  // namespace gold
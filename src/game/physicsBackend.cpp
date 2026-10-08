// The physicsBackend registry plus the built-in no-op "none" backend:
// worlds initialize to an empty simulation, so gold::game builds and
// runs on machines with no physics engine; the "bullet" backend ships
// as the loadable libgoldBullet plugin.

#include "game/physicsBackend.hpp"

#include <map>
#include <mutex>

#include "plugin.hpp"

namespace gold {

	namespace {
		std::mutex& registryMutex() {
			static std::mutex m;
			return m;
		}

		std::map<std::string, createPhysicsBackendFn>& factories() {
			static std::map<std::string, createPhysicsBackendFn> f;
			return f;
		}
	}  // namespace

	// The "none" backend: the guaranteed graceful fallback. Every call is
	// a no-op that keeps the facades' contracts (a body "succeeds" but
	// registers nothing; transforms stay under script control).
	namespace {
		class nonePhysicsBackend : public physicsBackend {
		 public:
			const char* name() const override { return "none"; }
			bool createWorld(object, func) override { return true; }
			void destroyWorld(object) override {}
			void setGravity(object, float[3]) override {}
			void step(object, float, int, float) override {}
			void debugDraw(object) override {}
			bool createShape(object) override { return true; }
			void destroyObject(object) override {}
			bool createBody(object, object) override { return true; }
		};

		struct noneRegistrar {
			noneRegistrar() {
				registerPhysicsBackend("none",
					[]() -> physicsBackend* {
						return new nonePhysicsBackend();
					});
			}
		};
		noneRegistrar noneReg;
	}  // namespace

	void registerPhysicsBackend(const std::string& name,
		createPhysicsBackendFn factory) {
		std::lock_guard<std::mutex> guard(registryMutex());
		factories()[name] = factory;
	}

	physicsBackend* createPhysicsBackend(const std::string& name) {
		plugin::addModulePath((void*)registerPhysicsBackend);
		{
			std::lock_guard<std::mutex> guard(registryMutex());
			auto it = factories().find(name);
			if (it != factories().end()) return it->second();
		}
		// Miss: a physics plugin may provide it (the bullet backend
		// self-registers on load). Loading runs outside the registry
		// mutex; stop at the first hit.
		for (const auto& candidate : plugin::pluginCandidates(name)) {
			if (!plugin::load(candidate)) continue;
			std::lock_guard<std::mutex> guard(registryMutex());
			auto it = factories().find(name);
			if (it != factories().end()) return it->second();
		}
		return nullptr;
	}

	physicsBackend* createPhysicsBackend(const list& names) {
		auto copy = names;
		for (auto it = copy.begin(); it != copy.end(); ++it) {
			auto name = it->getString();
			if (name.empty()) continue;
			if (auto* backend = createPhysicsBackend(name)) return backend;
		}
		return nullptr;
	}

}  // namespace gold
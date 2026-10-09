// The mcpServerSystem registry: no built-in implementation. The MCP
// service ships as the loadable libgoldMcp module (HTTP via the web
// module's transports); it self-registers under the name "mcp" when its
// code loads, so the engine reaches it only through this seam and only
// when the app's config asks for one.

#include "mcpServerSystem.hpp"

#include <map>
#include <mutex>

#include "plugin.hpp"

namespace gold {

	namespace {
		std::mutex& registryMutex() {
			static std::mutex m;
			return m;
		}

		std::map<std::string, createMcpServerFn>& factories() {
			static std::map<std::string, createMcpServerFn> f;
			return f;
		}
	}  // namespace

	void registerMcpServer(const std::string& name,
		createMcpServerFn factory) {
		std::lock_guard<std::mutex> guard(registryMutex());
		factories()[name] = factory;
	}

	mcpServerSystem* createMcpServer(const std::string& name) {
		plugin::addModulePath((void*)registerMcpServer);
		{
			std::lock_guard<std::mutex> guard(registryMutex());
			auto it = factories().find(name);
			if (it != factories().end()) return it->second();
		}
		// Miss: the mcp module may provide it (it self-registers on
		// load). Loading runs outside the registry mutex; stop at the
		// first hit.
		for (const auto& candidate : plugin::pluginCandidates(name)) {
			if (!plugin::load(candidate)) continue;
			std::lock_guard<std::mutex> guard(registryMutex());
			auto it = factories().find(name);
			if (it != factories().end()) return it->second();
		}
		return nullptr;
	}

	mcpServerSystem* createMcpServer(const list& names) {
		auto copy = names;
		for (auto it = copy.begin(); it != copy.end(); ++it) {
			auto name = it->getString();
			if (name.empty()) continue;
			if (auto* server = createMcpServer(name)) return server;
		}
		return nullptr;
	}

}  // namespace gold
// The server-transport registry plus the shared dispatch policy. All of
// this lives in the web module (transport-free); concrete transports are
// loadable plugins.

#include <iostream>
#include <map>
#include <mutex>

#include "plugin.hpp"
#include "serverTransport.hpp"
#include "session.hpp"
#include "web/server.hpp"

namespace gold {

	namespace {
		std::mutex& transportMutex() {
			static std::mutex m;
			return m;
		}

		std::map<std::string, createServerTransportFn>& transportFactories() {
			static std::map<std::string, createServerTransportFn> f;
			return f;
		}
	}  // namespace

	/** Call a gold route handler with the shared error policy: any
	 *  exception becomes a 500 reply on that connection. */
	var dispatchRoute(func handler, var reqV, var resV) {
		try {
			handler({reqV, resV});
			return var();
		} catch (genericError& e) {
			std::cerr << e << std::endl;
		} catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
		} catch (...) {
			std::cerr << "unknown exception in route handler" << std::endl;
		}
		auto res = resV.getObject<response>();
		if (res && !res.hasResponded().getBool()) {
			res.writeStatus(list({(uint16_t)500}));
			res.end(list());
		}
		return var();
	}

	void registerServerTransport(const std::string& name,
		createServerTransportFn factory) {
		std::lock_guard<std::mutex> guard(transportMutex());
		transportFactories()[name] = factory;
	}

	serverTransport* createServerTransport(const list& names) {
		plugin::addModulePath((void*)registerServerTransport);
		auto copy = names;
		for (auto it = copy.begin(); it != copy.end(); ++it) {
			auto name = it->getString();
			if (name.empty()) continue;
			{
				std::lock_guard<std::mutex> guard(transportMutex());
				auto fit = transportFactories().find(name);
				if (fit != transportFactories().end()) return fit->second();
			}
			// Miss: a transport plugin may provide it (self-registers on
			// load); loading happens outside the registry mutex.
			plugin::load(name);
			std::lock_guard<std::mutex> guard(transportMutex());
			auto fit = transportFactories().find(name);
			if (fit != transportFactories().end()) return fit->second();
		}
		return nullptr;
	}

}  // namespace gold
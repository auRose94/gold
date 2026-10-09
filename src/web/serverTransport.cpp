// The server-transport registry plus the shared dispatch policy. All of
// this lives in the web module (transport-free); concrete transports are
// loadable plugins.

#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>

#include "file.hpp"
#include "plugin.hpp"
#include "serverTransport.hpp"
#include "session.hpp"
#include "web/server.hpp"

namespace gold {
	using namespace std;
	namespace fs = std::filesystem;

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

	// The static-file mount handler: resolve the request path against
	// the server's buffered mounts (gold file objects) and answer with
	// the ETag/304/mime/cache-control policy; 404 on miss. Two lookup
	// relaxations match the previous per-transport copies: "/assets/"
	// prefixes strip to the asset path, and a URL ending in "/index.*"
	// rewrites to the directory it indexes.
	static auto mimeMap = map<string, string>({
		{".bin", "application/octet-stream"},
		{".zip", "application/zip"},
		{".rar", "application/x-rar-compressed"},
		{".json", "application/json"},
		{".bson", "application/bson"},
		{".js", "application/javascript"},
		{".xml", "application/xml"},
		{".gz", "application/gzip"},
		{".bz", "application/x-bzip"},
		{".bz2", "application/x-bzip2"},
		{".azw", "application/vnd.amazon.ebook"},
		{".doc", "application/msword"},
		{".ogx", "application/ogg"},
		{".pdf", "application/pdf"},
		{".tar", "application/x-tar"},
		{".xhtml", "application/xhtml+xml"},
		{".xls", "application/vnd.ms-excel"},
		{".7z", "application/x-7z-compressed"},
		{".abw", "application/x-abiword"},
		{".arc", "application/x-freearc"},
		{".html", "text/html"},
		{".htm", "text/html"},
		{".csv", "text/csv"},
		{".css", "text/css"},
		{".rtf", "text/rtf"},
		{".txt", "text/plain"},
		{".ics", "text/calendar"},
		{".xlsx",
		 "application/"
		 "vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
		{".ttf", "font/ttf"},
		{".woff", "font/woff"},
		{".woff2", "font/woff2"},
		{".png", "image/png"},
		{".jpg", "image/jpeg"},
		{".jpeg", "image/jpeg"},
		{".gif", "image/gif"},
		{".webp", "image/webp"},
		{".dds", "image/vnd-ms.dds"},
		{".wav", "audio/wav"},
		{".mp3", "audio/mpeg"},
		{".glb", "model/gltf-binary"},
		{".gltf", "model/gltf+json"},
	});

	func makeMountHandler(object host) {
		return func([host](list args) mutable -> var {
			auto req = args[0].getObject<request>();
			auto res = args[1].getObject<response>();
			auto p = string(req.getUrl());
			auto mounts = host.getObject("mounts");
			auto f = mounts.getObject<file>(p);
			if (!f) {
				auto assetIndex = p.find("/assets/");
				if (assetIndex != string::npos) {
					p = p.substr(0, assetIndex) +
						p.substr(assetIndex + 8);
					f = mounts.getObject<file>(p);
				}
				if (!f) {
					auto indexIndex = p.find("/index.");
					if (indexIndex != string::npos) {
						p = p.substr(0, indexIndex);
						f = mounts.getObject<file>(p);
					}
				}
			}
			auto chash =
				req.getHeader({"if-none-match"}).getString();
			auto control = host.getString("cacheControl");
			if (f) {
				auto loaded = f.load();
				if (loaded.isView()) {
					auto hash = f.hash().getString();
					if (hash == chash) {
						// "304 Not Modified".
						res.writeStatus(list({(uint16_t)304}));
						res.writeHeader(
							{"Cache-Control", control});
						res.end(list());
						return var();
					}
					auto bin = loaded.getBinary();
					res.writeStatus(list({(uint16_t)200}));
					// Find, not []: unknown types must not mutate
					// the shared map per request.
					auto ext = fs::path(p).extension().string();
					auto mimeIt = mimeMap.find(ext);
					const string ct = mimeIt != mimeMap.end()
										  ? mimeIt->second
										  : "application/octet-stream";
					res.writeHeader({"Content-Type", ct});
					res.writeHeader(
						{"Cache-Control", control});
					res.writeHeader({"ETag", hash});
					res.end(list({bin}));
					return var();
				}
				res.writeStatus(list({(uint16_t)404}));
				res.end(list());
				return var();
			}
			res.writeStatus(list({(uint16_t)404}));
			res.end(list());
			return var();
		});
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
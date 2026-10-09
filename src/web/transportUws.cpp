// The uWS transport: the stopgap HTTP engine behind gold's server
// facade, shipped as the loadable libgoldUws plugin until the
// libwebsockets port replaces it (queued phase). Every uWS-typed line
// lives here; the facade stays transport-free.

#include <fcntl.h>
#include <filesystem>

#include <App.h>

#include "file.hpp"
#include "goldjs.hpp"
#include "plugin.hpp"
#include "server.hpp"
#include "serverTransport.hpp"
#include "session.hpp"

namespace gold {
	using namespace std;
	using namespace uWS;
	namespace fs = std::filesystem;

	namespace {

		/** One connection: the response writer + (for yield) the request
		 *  reader, behind the gold session interface. */
		struct uwsSession : public session {
			HttpResponse<false>* res;
			HttpRequest* req;

			uwsSession(HttpResponse<false>* r, HttpRequest* q)
				: res(r), req(q) {}

			void setYield(bool value) override {
				if (req) req->setYield(value);
			}

			bool yielded() const override {
				return req ? req->getYield() : false;
			}

			bool writeContinue() override {
				res->writeContinue();
				return true;
			}

			bool writeStatusRaw(const string& status) override {
				res->writeStatus(status);
				return true;
			}

			bool writeHeaderRaw(const string& key, const string& value) override {
				res->writeHeader(key, value);
				return true;
			}

			bool end(const string& body) override {
				res->end(string_view(body.data(), body.size()));
				return true;
			}

			list tryEnd(const string& body, size_t totalSize) override {
				auto p = res->tryEnd(
					string_view(body.data(), body.size()), totalSize);
				return list({(int64_t)p.first, (int64_t)p.second});
			}

			size_t write(const string& partial) override {
				return res->write(string_view(partial.data(), partial.size()));
			}

			size_t writeOffset() const override {
				return res->getWriteOffset();
			}

			bool hasResponded() const override { return res->hasResponded(); }

			void cork(func handler) override {
				res->cork([handler]() { handler({}); });
			}

			void onWritable(func handler) override {
				res->onWritable([handler](int available) {
					return handler({available});
				});
			}

			void onAborted(func handler) override {
				res->onAborted([handler]() { handler({}); });
			}

			void onDataRaw(func handler) override {
				res->onData([handler](string_view raw, bool final) {
					handler({string(raw), final});
				});
			}
		};

		/** Stage the request's data fields onto the gold object (headers,
		 *  route parameters, query, method and URL). */
		void fillRequest(HttpRequest* httpReq, request& into) {
			auto headers = obj({});
			auto params = list({});
			auto it = httpReq->begin();
			for (; it != httpReq->end(); ++it) {
				if (it.ptr) {
					auto k = string(it.ptr->key);
					auto v = string(it.ptr->value);
					auto exist = headers.getVar(k);
					if (exist.isString()) {
						auto arr = list({exist.getString(), v});
						headers.setList(k, arr);
					} else if (exist.isList()) {
						auto arr = exist.getList();
						arr.pushString(v);
					} else
						headers.setString(k, v);
				}
			}
			uint32_t i = 0;
			auto currentParm = httpReq->getParameter(i);
			while (currentParm.data() && currentParm.size() > 0) {
				params.pushString(string(currentParm));
				currentParm = httpReq->getParameter(++i);
			}
			into.setObject("headers", headers);
			into.setList("params", params);
			into.setString("query", string(httpReq->getQuery()));
			into.setString("method", string(httpReq->getMethod()));
			into.setString("path", string(httpReq->getUrl()));
		}

		auto routeInto(func handler) {
			return [handler](auto* res, auto* req) {
				uwsSession sess(res, req);
				request rq(&sess);
				response rs(&sess);
				fillRequest(req, rq);
				dispatchRoute(handler, rq, rs);
			};
		}

		struct uwsTransport : public serverTransport {
			const char* name() const override { return "uws"; }

			var run(object host) override {
				// SSL settings are read for compatibility; this vintage
				// app is a plain (non-SSL) TemplatedApp, so keys are
				// inert — HTTPS arrives with the libwebsockets port.
				auto settings = us_socket_context_options_t{};

				auto app = App(settings);
				auto mounts = host.getObject("mounts");

				// The user error handler first so specific routes still
				// win; the automatic catch-alls land last.
				auto errorHandler = host.getFunc("errorHandler");
				if ((bool)errorHandler)
					app.get("/*", routeInto(errorHandler));

				for (auto it = mounts.begin(); it != mounts.end(); ++it) {
					auto url = it->first;
					app.get(url,
						routeInto(makeMountHandler(host)));
				}

				auto routes = host.getObject("routes");
				auto bind =
					[&app, &routes](const string& verb) {
						auto byVerb = routes.getObject(verb);
						if (!byVerb) return;
						for (auto it2 = byVerb.begin();
							 it2 != byVerb.end(); ++it2) {
							auto pattern = it2->first;
							auto handler = it2->second.getFunction();
							if (!handler) continue;
							if (verb == "get")
								app.get(pattern.c_str(), routeInto(handler));
							else if (verb == "post")
								app.post(pattern.c_str(), routeInto(handler));
							else if (verb == "put")
								app.put(pattern.c_str(), routeInto(handler));
							else if (verb == "patch")
								app.patch(pattern.c_str(), routeInto(handler));
							else if (verb == "del")
								// C++ del binds uWS's delete route.
								app.del(pattern.c_str(), routeInto(handler));
							else if (verb == "options")
								app.options(pattern.c_str(), routeInto(handler));
						}
					};
				bind("get");
				bind("post");
				bind("put");
				bind("patch");
				bind("del");
				bind("options");

				// Unmatched requests must not hang the client; user routes
				// and handlers registered earlier still match first.
				app.get("/*", [](auto* res, auto*) {
					res->writeStatus("404 Not Found");
					res->end();
				});
				app.post("/*", [](auto* res, auto*) {
					res->writeStatus("404 Not Found");
					res->end();
				});
				app.put("/*", [](auto* res, auto*) {
					res->writeStatus("404 Not Found");
					res->end();
				});
				app.patch("/*", [](auto* res, auto*) {
					res->writeStatus("404 Not Found");
					res->end();
				});
				app.del("/*", [](auto* res, auto*) {
					res->writeStatus("404 Not Found");
					res->end();
				});
				app.options("/*", [](auto* res, auto*) {
					res->writeStatus("404 Not Found");
					res->end();
				});

				const auto hostName = host.getString("host");
				const auto port = host.getInt32("port");
				bool bound = false;
				app.listen(hostName, port,
					[port, hostName, &bound](auto* listenSocket) {
						if (listenSocket) {
							bound = true;
							cout << "Serving " << hostName << " over "
								 << port << endl;
						}
					});
				// A failed bind must not fall through into `.run()`: the
				// loop would spin forever and report success to the caller.
				if (!bound)
					return genericError(
						"Failed to bind " + hostName + ":" + to_string(port));
				app.run();
				return var(true);
			}
		};

		struct uwsRegistrar {
			uwsRegistrar() {
				registerServerTransport("uws",
					[]() -> serverTransport* { return new uwsTransport(); });
			}
		};
		uwsRegistrar uwsReg;
	}  // namespace
}  // namespace gold
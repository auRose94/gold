// The libwebsockets transport: gold's loadable HTTP/WebSocket engine
// (backend "lws", plugin libgoldLws.so), linked against the system
// package. When the package is absent the build skips this plugin and
// the server facade degrades to "no server transport available".
//
// Dispatch order matches the uWS stopgap: verb routes (:param patterns,
// registration order) → mount fallbacks → the server's errorHandler →
// the automatic 404. Route handlers are dispatched on the service-loop
// thread; start() blocks on that thread until stop() is requested from
// any thread.

#include <libwebsockets.h>

#include <atomic>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "file.hpp"
#include "goldjs.hpp"
#include "plugin.hpp"
#include "server.hpp"
#include "serverTransport.hpp"
#include "session.hpp"

namespace gold {
	using namespace std;
	namespace fs = std::filesystem;

	namespace {

		/** The response body for a status line that carries no payload. */
		bool bodylessStatus(const string& statusLine) {
			const auto code = (int)atoi(statusLine.c_str());
			return code == 204 || code == 304;
		}

		/** Run a gold handler func without letting an exception unwind
		 *  into the service loop (the dispatchRoute policy, for the ws
		 *  and body callbacks that have no response to fail with). */
		void runGold(func handler, list args, const string& where) {
			try {
				handler(std::move(args));
			} catch (genericError& e) {
				std::cerr << e << std::endl;
			} catch (const std::exception& e) {
				std::cerr << e.what() << std::endl;
			} catch (...) {
				std::cerr << "unknown exception in " << where << std::endl;
			}
		}

		// ------------------------------------------------------------- conn

		// One connection: transport-side state behind BOTH the HTTP
		// request/response facades and the wsSocket facade. Lives in
		// lwsTransport's registry, keyed by wsi, from the connection's
		// first callback to WSI_DESTROY.
		struct lwsConn : public session, public wsConn {
			explicit lwsConn(struct lws* w) : wsi(w) {}

			struct lws* wsi;

			// ------------------------------------------------- http tx
			// The response staging: gold facades write through the
			// session interface; the pump flushes per transaction. Reset
			// per route candidate, then per request transaction.
			std::vector<std::pair<string, string>> headers;
			string statusLine{"200 OK"};
			string body;
			size_t bodySent = 0;
			bool responded = false;
			bool yieldNext = false;
			bool headersWritten = false;
			bool txOpen = false;
			bool requestPending = false;
			bool abortedFired = false;
			// Request-side fields staged once per transaction.
			string method;
			string path;
			string query;
			// Request-body streaming and response hooks.
			func onDataCb;
			func onWritableCb;
			func onAbortedCb;

			// --------------------------------------------------- ws
			bool wsUp = false;
			std::deque<std::pair<bool, string>> outbox;  // {binary, bytes}
			string wsRecv;                               // fragment assembly
			object handlers;                             // {open, message, close}
			object sock;                                 // the wsSocket facade

			void resetStaging() {
				statusLine = "200 OK";
				headers.clear();
				body.clear();
				bodySent = 0;
				responded = false;
				headersWritten = false;
			}

			// ---------------------------------------------------- session
			void setYield(bool value) override { yieldNext = value; }

			bool yielded() const override { return yieldNext; }

			bool writeContinue() override {
				// lws answers "Expect: 100-continue" itself once the
				// handler reads the body.
				return true;
			}

			bool writeStatusRaw(const string& status) override {
				statusLine = status;
				return true;
			}

			bool writeHeaderRaw(const string& key,
				const string& value) override {
				headers.emplace_back(key, value);
				return true;
			}

			bool end(const string& body) override {
				if (responded) return false;
				this->body = body;
				responded = true;
				if (!requestPending) lws_callback_on_writable(wsi);
				return true;
			}

			list tryEnd(const string& body, size_t totalSize) override {
				// No caller streams through tryEnd today; degenerate to
				// one fully-staged write.
				(void)totalSize;
				this->body += body;
				responded = true;
				if (!requestPending) lws_callback_on_writable(wsi);
				return list({(int64_t)1, (int64_t)this->body.size()});
			}

			size_t write(const string& partial) override {
				body += partial;
				responded = true;
				if (!requestPending) lws_callback_on_writable(wsi);
				return partial.size();
			}

			size_t writeOffset() const override { return bodySent; }

			bool hasResponded() const override { return responded; }

			void cork(func handler) override { handler({}); }

			void onWritable(func handler) override {
				onWritableCb = handler;
				if (!responded && txOpen) lws_callback_on_writable(wsi);
			}

			void onAborted(func handler) override { onAbortedCb = handler; }

			void onDataRaw(func handler) override { onDataCb = handler; }

			// --------------------------------------------------- wsConn
			bool sendRaw(const string& data, bool binary) override {
				if (!wsUp) return false;
				outbox.emplace_back(binary, data);
				lws_callback_on_writable(wsi);
				return true;
			}

			void closeConn() override {
				if (wsUp)
					lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, nullptr,
						0);
			}
		};

		// ----------------------------------------------- http flushing
		// Flush the response the facades staged on `c`. Idempotent across
		// the callbacks that drive it (post-dispatch, body completion,
		// HTTP_WRITEABLE): the header block writes once, synchronously;
		// the body goes out in socket-friendly chunks; then the
		// transaction completes (keep-alive reset, or close on
		// "Connection: close"). Returns nonzero to close the connection.
		int pumpHttp(struct lws* wsi, lwsConn& c) {
			// A handler that stages nothing yet (custom streaming drives
			// it later through write()) has nothing to do here.
			if (!c.responded || !c.txOpen || c.requestPending) return 0;
			if (!c.headersWritten) {
				size_t need = 64;
				for (auto& h : c.headers)
					need += h.first.size() + h.second.size() + 6;
				string block(LWS_PRE + need, 0);
				auto* start = (unsigned char*)block.data() + LWS_PRE;
				auto* p = start;
				auto* endP = (unsigned char*)block.data() + block.size();
				auto code = (unsigned int)atoi(c.statusLine.c_str());
				if (code == 0) code = 200;
				if (lws_add_http_header_status(wsi, code, &p, endP))
					return -1;
				for (auto& h : c.headers) {
					auto nm =
						h.first.back() == ':' ? h.first : h.first + ":";
					if (lws_add_http_header_by_name(wsi,
							(const unsigned char*)nm.data(),
							(const unsigned char*)h.second.data(),
							(int)h.second.size(), &p, endP))
						return -1;
				}
				if (!bodylessStatus(c.statusLine))
					if (lws_add_http_header_content_length(wsi,
							(lws_filepos_t)c.body.size(), &p, endP))
						return -1;
				if (lws_finalize_write_http_header(wsi, start, &p, endP))
					return -1;
				c.headersWritten = true;
			}
			// Body chunks in ~2*mtu writes: lws_write accepts the whole
			// chunk (buffering any OS remainder itself) or the connection
			// is dead.
			while (c.bodySent < c.body.size()) {
				if (lws_send_pipe_choked(wsi)) {
					lws_callback_on_writable(wsi);
					return 0;
				}
				const auto remaining = c.body.size() - c.bodySent;
				const auto budget =
					remaining < 2048 ? remaining : (size_t)2048;
				string buf(LWS_PRE + budget, 0);
				// The replace copies the chunk in at LWS_PRE, keeping
				// lws's expected framing pad intact.
				buf.replace(LWS_PRE, budget, c.body, c.bodySent, budget);
				const bool last = c.bodySent + budget >= c.body.size();
				// The payload pointer must sit past the LWS_PRE pad so
				// lws's frame headers land in the pad, not the string.
				const auto n = lws_write(wsi,
					(unsigned char*)buf.data() + LWS_PRE, budget,
					last ? LWS_WRITE_HTTP_FINAL : LWS_WRITE_HTTP);
				if (n < 0) return -1;
				// lws_write accepts the whole chunk or fails; a 0 means
				// it took nothing this pass — retry on the next writable.
				if (n == 0) {
					lws_callback_on_writable(wsi);
					return 0;
				}
				c.bodySent += budget;
			}
			if (c.bodySent >= c.body.size()) {
				const auto done = lws_http_transaction_completed(wsi);
				c.txOpen = false;
				return done;
			}
			return 0;
		}

		/** Stage a bare status response (the automatic 404/unrouted reply). */
		void answerSimple(lwsConn& c, uint16_t code) {
			c.statusLine = httpStatusLine(code);
			c.body.clear();
			c.responded = true;
		}

		// ------------------------------------------------- request fill
		// Stage the request's data fields onto the gold object: headers
		// from lws's parsed token table (the method tokens carry the
		// request line and are skipped), query/method/path staged by the
		// connection.
		void fillRequest(lwsConn& c, request& into, list params) {
			auto headers = obj({});
			for (auto t = 0; t < WSI_TOKEN_COUNT; ++t) {
				const auto tok = (enum lws_token_indexes)t;
				const int len = lws_hdr_total_length(c.wsi, tok);
				if (len <= 0) continue;
				auto namePtr = lws_token_to_string(tok);
				if (!namePtr) continue;
				auto name = string((const char*)namePtr);
				const auto colon = name.find(':');
				auto bare = colon == string::npos
								? name
								: name.substr(0, colon);
				// Method tokens like "get-uri:" hold the request line.
				if (bare.size() >= 3 &&
					bare.compare(bare.size() - 3, 3, "uri") == 0)
					continue;
				auto value = make_unique<char[]>(len + 1);
				if (lws_hdr_copy(c.wsi, value.get(), len + 1, tok) < 0)
					continue;
				auto existing = headers.getVar(bare);
				if (existing.isList()) {
					auto arr = existing.getList();
					arr.pushString(string(value.get()));
					headers.setList(bare, arr);
				} else if (existing.isString()) {
					headers.setList(bare,
						list({existing.getString(),
							string(value.get())}));
				} else
					headers.setString(bare, string(value.get()));
			}
			into.setObject("headers", headers);
			into.setList("params", params);
			into.setString("query", c.query);
			into.setString("method", c.method);
			into.setString("path", c.path);
		}

		// ------------------------------------------------------- ws pump
		// Flush the staged outbox; lws_write queues or fails.
		int pumpWs(struct lws* wsi, lwsConn& c) {
			if (!c.wsUp) return 0;
			while (!c.outbox.empty()) {
				const auto& pending = c.outbox.front();
				string buf(LWS_PRE + pending.second.size(), 0);
				buf.replace(LWS_PRE, string::npos, pending.second);
				// Payload pointer past the pad (see pumpHttp).
				const auto n = lws_write(wsi,
					(unsigned char*)buf.data() + LWS_PRE,
					pending.second.size(),
					pending.first ? LWS_WRITE_BINARY : LWS_WRITE_TEXT);
				if (n < 0) return -1;
				c.outbox.pop_front();
			}
			return 0;
		}

		// ------------------------------------------------ route matching

		/** Split a URL into '/' segments past a leading one ("" for "/"
		 *  itself). */
		std::vector<string> pathSegments(const string& url) {
			std::vector<string> out;
			auto pos = !url.empty() && url[0] == '/' ? 1u : 0u;
			while (true) {
				const auto next = url.find('/', pos);
				if (next == string::npos) {
					out.push_back(url.substr(pos));
					return out;
				}
				out.push_back(url.substr(pos, next - pos));
				pos = next + 1;
			}
		}

		/** uWS-style pattern match: ':'-prefixed segments capture one path
		 *  segment, a trailing "*" captures the rest including slashes
		 *  (the wildcard-fallback form); the rest must match
		 *  literally. */
		bool matchPattern(const string& pattern, const string& path,
			list& params) {
			if (path.size() < 1 || path[0] != '/') return false;
			auto p = pathSegments(pattern);
			auto t = pathSegments(path);
			for (size_t i = 0; i < p.size(); ++i) {
				const auto& seg = p[i];
				if (seg == "*" && i + 1 == p.size()) {
					// Trailing wildcard: capture what is left, slashes
					// included.
					string rest;
					for (auto j = i; j < t.size(); ++j) {
						if (j > i) rest += "/";
						rest += t[j];
					}
					params.pushString(rest);
					return true;
				}
				if (seg.size() > 1 && seg[0] == ':') {
					if (i >= t.size()) return false;
					params.pushString(t[i]);
					continue;
				}
				if (i >= t.size() || seg != t[i]) return false;
			}
			return p.size() == t.size();
		}

		/** One route candidate: fresh request/response facades around the
		 *  connection, params staged, the handler run under the shared
		 *  error policy. True when the handler took over the response. */
		bool runCandidate(lwsConn& c, func handler, list params) {
			c.resetStaging();
			request rq(&c);
			response rs(&c);
			fillRequest(c, rq, params);
			dispatchRoute(handler, rq, rs);
			return c.responded;
		}

	}  // namespace

	// --------------------------------------------------------- transport

	class lwsTransport : public serverTransport {
	public:
		const char* name() const override { return "lws"; }

		void stop() override {
			stopping.store(true, std::memory_order_release);
		}

		var run(object serverHost) override {
			// Loop chatter stays out of the app's output.
			lws_set_log_level(LLL_ERR | LLL_WARN, nullptr);

			host = serverHost;

			// One protocol serves plain HTTP and WebSocket upgrades: lws
			// falls back to the first protocol for upgrades that name no
			// other subprotocol, and this protocol carries `this` so
			// every callback reaches the registry/state below.
			struct lws_protocols table;
			table.name = "gold";
			table.callback = &lwsTransport::lwsCallback;
			table.per_session_data_size = 0;
			table.rx_buffer_size = 4096;
			table.id = 0;
			table.user = (void*)this;
			table.tx_packet_size = 0;
			protocols[0] = table;
			protocols[1] = {};
			protocols[1].name = nullptr;
			protocols[1].callback = nullptr;

			struct lws_context_creation_info info {};
			auto hostName = host.getString("host");
			const auto port = host.getInt32("port");
			// The standard implicit-vhost form: port + iface at context
			// creation. The two-step form (NO_LISTEN context +
			// create_vhost) was observed to serve nothing on this lws
			// build; here a failed bind surfaces as a NULL context.
			info.port = port;
			info.protocols = protocols;
			info.iface = hostName.empty() ? nullptr : hostName.c_str();
			auto cert = host.getString("sslCert");
			auto key = host.getString("sslKey");
			const bool ssl = !cert.empty() && !key.empty();
			if (ssl) info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
			info.ssl_cert_filepath = ssl ? cert.c_str() : nullptr;
			info.ssl_private_key_filepath = ssl ? key.c_str() : nullptr;

			stopping.store(false, std::memory_order_release);
			auto context = lws_create_context(&info);
			// (lws logs the errno itself; harmless device-bind noise from
			// iface strings that parse as addresses is expected.)
			if (!context)
				return genericError("Failed to bind " + hostName + ":" +
					to_string(port));
			cout << "Serving " << hostName << " over " << port << endl;

			bool clean = true;
			while (clean && !stopping.load(std::memory_order_acquire)) {
				if (lws_service(context, 20) < 0) clean = false;
			}
			// Teardown callbacks (CLOSED/WSI_DESTROY) fire in here; the
			// conns registry outlives them.
			lws_context_destroy(context);
			conns.clear();
			return clean ? var(true)
						 : var(genericError("lws service loop failed"));
		}

		lwsConn& connFor(struct lws* wsi) {
			auto& slot = conns[wsi];
			if (!slot) slot.reset(new lwsConn(wsi));
			return *slot;
		}

		// Alive for the context's lifetime; read only on the service
		// thread (stop() touches no fields but the atomic flag).
		std::atomic<bool> stopping{false};
		object host;
		std::map<struct lws*, std::unique_ptr<lwsConn>> conns;
		struct lws_protocols protocols[2];

	private:
		static int lwsCallback(struct lws* wsi,
			enum lws_callback_reasons reason, void* user, void* in,
			size_t len) {
			(void)user;
			auto proto = lws_get_protocol(wsi);
			auto* self =
				proto ? (lwsTransport*)proto->user : nullptr;
			if (!self) return 0;

			switch (reason) {
			case LWS_CALLBACK_WSI_DESTROY:
				self->conns.erase(wsi);
				return 0;

			case LWS_CALLBACK_HTTP: {
				auto& c = self->connFor(wsi);
				c.onDataCb = func();
				c.onWritableCb = func();
				c.onAbortedCb = func();
				c.yieldNext = false;
				c.abortedFired = false;
				c.txOpen = true;
				c.requestPending = false;
				// lws tokenizes the request line into the method-named
				// token; unknown methods report -1.
				char* uri = nullptr;
				int uriLen = 0;
				const int meth =
					lws_http_get_uri_and_method(wsi, &uri, &uriLen);
				if (meth >= 0 && uri) {
					auto raw = string(uri, (size_t)uriLen);
					const auto hashPos = raw.find('?');
					c.path = raw.substr(0, hashPos);
					c.query =
						hashPos == string::npos ? "" : raw.substr(hashPos + 1);
					const char* verb = nullptr;
					switch (meth) {
					case LWSHUMETH_GET: verb = "get";
						break;
					case LWSHUMETH_POST: verb = "post";
						break;
					case LWSHUMETH_OPTIONS: verb = "options";
						break;
					case LWSHUMETH_PUT: verb = "put";
						break;
					case LWSHUMETH_PATCH: verb = "patch";
						break;
					case LWSHUMETH_DELETE: verb = "del";
						break;
					default: break;
					}
					c.method = verb ? verb : "";
				} else
					c.method = "";

				bool scanning = !c.method.empty();
				// 1. The verb's specific routes, registration order.
				if (scanning) {
					auto routes = self->host.getObject("routes");
					if (routes) {
						auto byVerb = routes.getObject(c.method);
						if (byVerb) {
							for (auto it = byVerb.begin();
								 it != byVerb.end(); ++it) {
								auto params = list({});
								if (!matchPattern(
										it->first, c.path, params))
									continue;
								auto handler =
									it->second.getFunction();
								if (!handler) continue;
								if (runCandidate(c, handler, params)) {
									scanning = false;
									break;
								}
								// Unanswered handlers fall through only
								// on an explicit yield; otherwise the
								// chain stops here (the stopgap
								// transport's behavior).
								if (c.yieldNext) c.yieldNext = false;
								else {
									scanning = false;
									break;
								}
							}
						}
					}
				}
				// 2. Mount fallbacks (the shared static-file policy).
				if (scanning && !c.responded) {
					c.yieldNext = false;
					if (runCandidate(c, makeMountHandler(self->host),
							list({string()})))
						scanning = false;
				}
				// 3. The user error handler (also the 404-page hook).
				if (scanning && !c.responded) {
					c.yieldNext = false;
					auto errorHandler = self->host.getFunc("errorHandler");
					if ((bool)errorHandler) {
						auto params = list({c.path});
						if (runCandidate(c, errorHandler, params))
							scanning = false;
					}
				}
				// 4. The automatic 404 for scans that ran to completion.
				// (A mid-chain break answers nothing, like the stopgap.)
				if (scanning && !c.responded) answerSimple(c, 404);
				return pumpHttp(wsi, c);
			}

			case LWS_CALLBACK_HTTP_BODY: {
				auto& c = self->connFor(wsi);
				if (c.onDataCb)
					runGold(c.onDataCb,
						{string((const char*)in, len), false},
						"request body");
				return 0;
			}

			case LWS_CALLBACK_HTTP_BODY_COMPLETION: {
				auto& c = self->connFor(wsi);
				c.requestPending = false;
				if (c.onDataCb)
					runGold(c.onDataCb, {string(), true}, "request body");
				return pumpHttp(wsi, c);
			}

			case LWS_CALLBACK_HTTP_WRITEABLE:
				return pumpHttp(wsi, self->connFor(wsi));

			// ------------------------------------------------- ws side

			case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION: {
				// Our handshake: route the upgrade against the buffered
				// ws routes ("ws" verb-object in routes).
				const auto uriLen =
					lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI);
				if (uriLen <= 0) return -1;
				auto raw = make_unique<char[]>(uriLen + 1);
				if (lws_hdr_copy(wsi, raw.get(), uriLen + 1,
						WSI_TOKEN_GET_URI) < 0)
					return -1;
				auto uri = string(raw.get());
				const auto hashPos = uri.find('?');
				auto path = hashPos == string::npos
								? uri
								: uri.substr(0, hashPos);
				auto routes = self->host.getObject("routes");
				if (!routes) return -1;
				auto wsRoutes = routes.getObject("ws");
				if (!wsRoutes) return -1;
				for (auto it = wsRoutes.begin(); it != wsRoutes.end();
					 ++it) {
					auto params = list({});
					if (!matchPattern(it->first, path, params)) continue;
					auto handlers = it->second.getObject();
					if (!handlers) continue;
					auto& c = self->connFor(wsi);
					c.handlers = handlers;
					c.path = path;
					return 0;
				}
				return -1;  // no ws route here: drop the handshake
			}

			case LWS_CALLBACK_ESTABLISHED: {
				auto& c = self->connFor(wsi);
				c.wsUp = true;
				if ((bool)c.handlers.getFunc("open")) {
					c.sock = wsSocket(&c, c.path);
					runGold(c.handlers.getFunc("open"), {c.sock}, "ws open");
				}
				return 0;
			}

			case LWS_CALLBACK_RECEIVE: {
				auto& c = self->connFor(wsi);
				if (!c.wsUp) return 0;
				c.wsRecv.append((const char*)in, len);
				if (!lws_is_final_fragment(wsi)) return 0;
				auto data = c.wsRecv;
				c.wsRecv.clear();
				if ((bool)c.handlers.getFunc("message"))
					runGold(c.handlers.getFunc("message"), {c.sock, data},
						"ws message");
				return 0;
			}

			case LWS_CALLBACK_SERVER_WRITEABLE:
				return pumpWs(wsi, self->connFor(wsi));

			case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
				// Returning 0 echoes the close and lws closes the conn;
				// the CLOSED event delivers the gold close handler.
				return 0;

			case LWS_CALLBACK_CLOSED: {
				auto& c = self->connFor(wsi);
				if (!c.abortedFired) {
					c.abortedFired = true;
					if (c.wsUp) {
						if ((bool)c.handlers.getFunc("close"))
							runGold(c.handlers.getFunc("close"), {c.sock},
								"ws close");
					} else if (c.onAbortedCb)
						runGold(c.onAbortedCb, {}, "response aborted");
				}
				return 0;
			}

			default:
				return 0;
			}
		}
	};

	namespace {
		struct lwsRegistrar {
			lwsRegistrar() {
				registerServerTransport("lws",
					[]() -> serverTransport* {
						return new lwsTransport();
					});
			}
		};
		lwsRegistrar lwsReg;
	}  // namespace
}  // namespace gold
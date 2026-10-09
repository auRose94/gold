// The mcpServer facade: an embeddable Model Context Protocol endpoint
// over the web module's server facade (own thread, buffered "/"-route,
// config-activated). The engine reaches it through the
// mcpServerSystem seam below ("mcp" config section); apps and tests
// may link gold::mcp and drive it directly.
//
// Execution model: the endpoint runs on the transport's service
// thread, but tool results are produced either there (engine-less
// apps) or by the host calling pump() from inside its loop (the gold
// frame, the safe point where nothing mid-frame is half-mutated).

#include "mcp/mcpServer.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "goldjs.hpp"
#include "goldjson.hpp"
#include "mcp/mcpDispatcher.hpp"
#include "mcpServerSystem.hpp"
#include "mcpTools.hpp"
#include "server.hpp"

namespace gold {
	using namespace std;

	namespace {

		/** One queued request, awaiting a frame pump ("frame" mode). */
		struct mcpJob {
			var message;
			var reply;
			bool done{false};
		};

		/** Everything the facade delegates to; kept in the object's
		 *  storage via setPtr so facade copies (handles) share one
		 *  server, like the web server facade's transport handle. */
		struct mcpImpl {
			object globals;     // roots + eval globals (one shared world)
			bool evalEnabled{true};
			// execute: "frame" | "server" | "auto" (frame as soon as a
			// pump has been seen; inline before that).
			bool frameMode{false};
			bool modeExplicit{false};
			int64_t timeoutMs{10000};
			atomic<bool> pumping{false};
			atomic<bool> stopping{false};
			bool listening{false};
			mutex m;
			condition_variable cv;
			deque<shared_ptr<mcpJob>> jobs;
			thread loop;
		};

		mcpServerContext contextOf(mcpImpl* impl) {
			mcpServerContext ctx;
			ctx.globals = impl->globals;
			ctx.evalEnabled = impl->evalEnabled;
			return ctx;
		}

		// ---------------------------------------------------- endpoint

		/** The whole POST /mcp exchange once the body is complete. All
		 *  of this runs on the transport's service thread; the only
		 *  code that leaves it is a tool call dispatched by pump(). */
		void mcpRespond(mcpImpl* impl, const string& buffer,
			response& res) {
			if (impl->stopping) return;
			auto message = jsonParse(buffer);
			if (message.isError() || !message.isObject()) {
				res.writeStatus({(uint16_t)400});
				res.end({mcpErrorEnvelope(var(), -32700,
					"malformed JSON: " + (string)message)});
				return;
			}
			// Notifications need no execution (and no Mcp-Session-Id
			// exists to check — see mcpDispatcher.hpp): acknowledge
			// with an empty 202.
			if (message.getObject().getVar("id").getType() == typeNull) {
				res.writeStatus({(uint16_t)202});
				res.end({});
				return;
			}
			bool frame = impl->frameMode;
			if (!impl->modeExplicit && impl->pumping.load()) frame = true;
			auto id = message.getObject().getVar("id");
			if (frame) {
				auto job = make_shared<mcpJob>();
				job->message = message;
				{
					unique_lock<mutex> lock(impl->m);
					if (impl->stopping) return;
					impl->jobs.push_back(job);
				}
				bool ready = false;
				var reply;
				{
					unique_lock<mutex> lock(impl->m);
					ready = impl->cv.wait_for(lock,
						chrono::milliseconds(impl->timeoutMs),
						[&] {
							return job->done || impl->stopping.load();
						});
					if (ready) reply = job->reply;
				}
				if (impl->stopping) return;  // the listener is dying
				if (!ready) {
					res.writeStatus({(uint16_t)503});
					res.end({mcpErrorEnvelope(id, -32001,
						"queued job timed out waiting for the engine "
						"frame (is the engine loop running?)")});
					return;
				}
				res.writeStatus({(uint16_t)200});
				res.end({reply});
				return;
			}
			// "server" mode: run inline on the service thread (gold's
			// data layer is per-field mutexed, and engine-less apps run
			// nothing else meanwhile).
			auto ctx = contextOf(impl);
			auto reply = mcpDispatch(message, ctx);
			res.writeStatus({(uint16_t)200});
			res.end({reply});
		}

	}  // namespace

	// -------------------------------------------------------- facade

	object& mcpServer::getPrototype() {
		static auto proto = obj({
			{"host", "127.0.0.1"},
			{"port", 8090},
			{"transport", "lws"},
			{"eval", true},
			{"execute", "auto"},
			{"timeout", 10000},
			{"initialize", method(&mcpServer::initialize)},
			{"start", method(&mcpServer::start)},
			{"stop", method(&mcpServer::stop)},
			{"setRoots", method(&mcpServer::setRoots)},
			{"attach", method(&mcpServer::attach)},
			{"pump", method(&mcpServer::pump)},
			{"destroy", method(&mcpServer::destroy)},
		});
		return proto;
	}

	mcpServer::mcpServer() : obj() {
		setParent(getPrototype());
		initialize({});
	}

	mcpServer::mcpServer(initList config) : obj(config) {
		setParent(getPrototype());
		initialize({});
	}

	mcpServer::mcpServer(object config) : obj() {
		copy(config);
		setParent(getPrototype());
		initialize({});
	}

	var mcpServer::initialize(list) {
		// Re-initialization shuts the previous endpoint down first.
		stop({});
		if (auto* old = (mcpImpl*)getPtr("impl")) {
			delete old;
			setPtr("impl", nullptr);
		}

		auto impl = new mcpImpl();
		impl->evalEnabled = getBool("eval", true);
		auto mode = getString("execute", "auto");
		impl->modeExplicit = mode != "auto";
		impl->frameMode = mode == "frame";
		impl->timeoutMs = getInt64("timeout", 10000);
		setPtr("impl", impl);

		// The endpoint rides gold's own server facade (the loadable
		// lws transport by default). Routes are functions carrying the
		// impl pointer; the facade keeps the web server as a member,
		// so start/stop reach it by name.
		server inner(jo("host", getString("host", "127.0.0.1"),
			"port", getInt64("port", 8090),
			"transport", getString("transport", "lws")));
		setObject("server", inner);
		inner.post({string("/mcp"),
			func([impl](list args) -> var {
				auto req = args[0].getObject<request>();
				auto res = args[1].getObject<response>();
				// The body streams to the endpoint; gold's response
				// facade buffers it until the final chunk.
				res.onData(ja(
					func([impl](list stream) -> var {
						auto buffer = stream[0].getString();
						auto bodyRes =
							stream[2].getObject<response>();
						mcpRespond(impl, buffer, bodyRes);
						return var();
					}),
					var(req)));
				return var();
			})});
		// GET /mcp: the endpoint advertises POST/DELETE only (there is
		// no SSE response stream in this server yet).
		inner.get({string("/mcp"),
			func([](list args) -> var {
				auto res = args[1].getObject<response>();
				res.writeHeader({"Allow", "POST, DELETE"});
				res.writeStatus({(uint16_t)405});
				res.end({});
				return var();
			})});
		// DELETE /mcp: session teardown. This server does not mint
		// session ids; keep the protocol's delete legal and idempotent.
		inner.del({string("/mcp"),
			func([](list args) -> var {
				auto res = args[1].getObject<response>();
				res.writeStatus({(uint16_t)204});
				res.end({});
				return var();
			})});
		setBool("initialized", true);
		return var();
	}

	var mcpServer::start(list) {
		auto impl = (mcpImpl*)getPtr("impl");
		if (!impl) return genericError("the MCP server is not initialized");
		if (impl->listening) return var(true);
		auto inner = getObject("server");
		impl->stopping = false;
		impl->listening = true;
		impl->loop = thread([inner]() mutable {
			// Blocks on the transport's loop until stop() is requested
			// from any thread (the web server facade's lifecycle).
			inner.callMethod("start");
		});
		return var(true);
	}

	var mcpServer::stop(list) {
		auto impl = (mcpImpl*)getPtr("impl");
		if (!impl) return var(false);
		const bool running = impl->listening;
		impl->stopping = true;
		{
			// Wake frame-waiters so their handler returns promptly and
			// the loop can exit.
			unique_lock<mutex> lock(impl->m);
		}
		impl->cv.notify_all();
		auto inner = getObject("server");
		if (inner) inner.callMethod("stop");
		if (impl->loop.joinable()) impl->loop.join();
		impl->listening = false;
		return var(running);
	}

	var mcpServer::setRoots(list args) {
		auto impl = (mcpImpl*)getPtr("impl");
		if (!impl) return genericError("the MCP server is not initialized");
		auto roots = args[0].getObject();
		if (!roots)
			return genericError(
				"setRoots expects an object of name -> gold object");
		for (auto it = roots.begin(); it != roots.end(); ++it)
			impl->globals.setVar(it->first, it->second);
		return var();
	}

	var mcpServer::attach(list args) {
		auto impl = (mcpImpl*)getPtr("impl");
		if (!impl) return genericError("the MCP server is not initialized");
		if (args.size() < 2)
			return genericError("attach expects (name, object)");
		impl->globals.setVar(args[0].getString(), args[1]);
		return var();
	}

	var mcpServer::pump(list) {
		auto impl = (mcpImpl*)getPtr("impl");
		if (!impl) return var();
		impl->pumping = true;
		auto ctx = contextOf(impl);
		while (true) {
			shared_ptr<mcpJob> job;
			{
				unique_lock<mutex> lock(impl->m);
				if (impl->stopping) break;
				if (impl->jobs.empty()) break;
				job = impl->jobs.front();
				impl->jobs.pop_front();
			}
			// Tool errors are results; a genuine dispatch-side failure
			// must still answer the waiting client, not kill the
			// engine loop calling this pump.
			var reply;
			try {
				reply = mcpDispatch(job->message, ctx);
			} catch (const exception& e) {
				reply = mcpErrorEnvelope(
					job->message.getObject().getVar("id"), -32603,
					string("dispatch failed: ") + e.what());
			}
			{
				unique_lock<mutex> lock(impl->m);
				job->reply = reply;
				job->done = true;
			}
			impl->cv.notify_all();
		}
		return var();
	}

	var mcpServer::destroy(list) {
		stop({});
		if (auto* impl = (mcpImpl*)getPtr("impl")) delete impl;
		erase("impl");
		erase("server");
		setBool("initialized", false);
		return var();
	}

	// --------------------------------------------------- engine seam

	namespace {
		/** The seam the engine loads: the facade behind the thin C++
		 *  interface. Its own dtor runs the facade's destroy (stop +
		 *  free) so the engine only needs shutdown() + delete. */
		class goldMcpSystem : public mcpServerSystem {
		 public:
			mcpServer facade;
			const char* name() const override { return "mcp"; }
			var initialize(object config) override {
				for (auto it = config.begin(); it != config.end(); ++it)
					facade.setVar(it->first, it->second);
				facade.initialize({});
				// Start listening right away: clients may arrive before
				// the engine loop starts (tool calls queue until the
				// first pump — see mcpServerContext's execute modes).
				facade.start({});
				return var();
			}
			void setRoots(object roots) override {
				facade.setRoots({roots});
			}
			void pump() override { facade.pump({}); }
			var shutdown() override { return facade.stop({}); }
			~goldMcpSystem() override { facade.destroy({}); }
		};

		struct registrar {
			registrar() {
				registerMcpServer("mcp",
					[]() -> mcpServerSystem* {
						return new goldMcpSystem();
					});
			}
		};
		registrar mcpRegistrar;
	}  // namespace

}  // namespace gold
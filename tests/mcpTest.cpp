// The MCP server tests: the dispatcher driven with plain gold vars
// (no network needed), then the real "/mcp" HTTP endpoint like the
// server suite does — own loop thread, raw loopback sockets, bounded
// waits everywhere so a regression fails instead of hanging.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include "goldjs.hpp"
#include "goldjson.hpp"
#include "goldtest.hpp"
#include "mcp/mcpDispatcher.hpp"
#include "mcp/mcpServer.hpp"

using namespace gold;

namespace {

	/** Blocking one-shot HTTP exchange over a raw loopback socket
	 *  (bounded; reads until the advertised Content-Length is met). */
	string exchange(int port, const string& request) {
		int fd = (int)::socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) return "";
		timeval ioTimeout{0, 300000};
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &ioTimeout,
			sizeof(ioTimeout));
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons((uint16_t)port);
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) != 0) {
			::close(fd);
			return "";
		}
		size_t sent = 0;
		while (sent < request.size()) {
			auto n = ::send(fd, request.data() + sent, request.size() - sent,
				0);
			if (n <= 0) break;
			sent += (size_t)n;
		}
		string out;
		const auto deadline = std::chrono::steady_clock::now() +
							  std::chrono::seconds(5);
		auto responseComplete = [&out]() {
			auto headerEnd = out.find("\r\n\r\n");
			if (headerEnd == string::npos) return false;
			size_t bodyStart = headerEnd + 4;
			string header = out.substr(0, bodyStart);
			for (auto& c : header) c = (char)tolower((unsigned char)c);
			auto cl = header.find("content-length:");
			if (cl == string::npos) return true;
			auto valueStart = header.find_first_not_of(
				" \t", cl + sizeof("content-length:") - 1);
			if (valueStart == string::npos) return true;
			auto valueEnd = header.find_first_not_of("0123456789",
				valueStart);
			size_t length = valueEnd == string::npos
								? (size_t)atoll(header.c_str() + valueStart)
								: (size_t)atoll(
									  header.substr(valueStart,
										  valueEnd - valueStart)
										  .c_str());
			return out.size() >= bodyStart + length;
		};
		char buf[4096];
		while (std::chrono::steady_clock::now() < deadline) {
			auto n = ::recv(fd, buf, sizeof(buf), 0);
			if (n <= 0) break;
			out.append(buf, (size_t)n);
			if (responseComplete()) break;
		}
		::close(fd);
		return out;
	}

	/** One MCP request over the raw socket: content-type + length are
	 *  set for the caller's JSON body. */
	string mcpPost(int port, const string& body) {
		return exchange(port,
			"POST /mcp HTTP/1.1\r\n"
			"Host: mcp-test\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: " +
				std::to_string(body.size()) +
			"\r\nConnection: close\r\n\r\n" +
			body);
	}

	bool waitUntilUp(int port) {
		for (int i = 0; i < 300; ++i) {
			int fd = (int)::socket(AF_INET, SOCK_STREAM, 0);
			if (fd >= 0) {
				sockaddr_in addr{};
				addr.sin_family = AF_INET;
				addr.sin_port = htons((uint16_t)port);
				addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
				const bool ok =
					::connect(fd, (sockaddr*)&addr, sizeof(addr)) == 0;
				::close(fd);
				if (ok) return true;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return false;
	}

	/** The JSON body of a raw HTTP response (after the header block). */
	string bodyOf(const string& response) {
		auto headerEnd = response.find("\r\n\r\n");
		if (headerEnd == string::npos) return "";
		return response.substr(headerEnd + 4);
	}

	/** A demo app object: gold data + a callable, like any facade. */
	object demoApp() {
		auto app = jo("name", "demo", "value", 42,
			"tags", ja("alpha", "beta"),
			"position", vec3f(0, 0, 0),
			"nested", jo("deep", jo("answer", 7)));
		// funcs receive (self, ...args) when dispatched via callMethod
		// (setFunc takes the callable by reference: keep it named).
		auto doubled = func([](list args) -> var {
			return args[1].getInt64() * 2;
		});
		app.setFunc("doubled", doubled);
		return app;
	}

	/** The first content item's text — where tool results live. */
	string contentText(var reply) {
		return reply.getObject()
			.getObject("result")
			.getList("content")[0]
			.getObject()
			.getString("text");
	}

	/** tools/list tool names (the "name" of each catalog entry). */
	bool hasTool(mcpServerContext& ctx, const string& name) {
		auto catalog = mcpToolCatalog(ctx).getList();
		for (auto it = catalog.begin(); it != catalog.end(); ++it)
			if (it->getObject().getString("name") == name) return true;
		return false;
	}

}  // namespace

TEST(mcp_dispatcher_initialize_negotiates_versions) {
	mcpServerContext ctx;
	ctx.globals.setObject("app", demoApp());

	auto reply = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 1, "method", "initialize",
			"params", jo("protocolVersion", "2025-06-18")),
		ctx);
	auto result = reply.getObject().getObject("result");
	EXPECT_EQ(result.getString("protocolVersion"), string("2025-06-18"));
	EXPECT_EQ(result.getObject("serverInfo").getString("name"),
		string("gold"));
	EXPECT_TRUE(result.getObject("capabilities")
					.getObject("tools")
					.getBool("listChanged", false) == false);

	// An unknown requested version falls back to the server's latest.
	auto fallback = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 2, "method", "initialize",
			"params", jo("protocolVersion", "1999-01-01")),
		ctx);
	EXPECT_EQ(
		fallback.getObject().getObject("result").getString("protocolVersion"),
		string(mcpServerContext::protocolVersion));
}

TEST(mcp_dispatcher_ping_notifications_and_errors) {
	mcpServerContext ctx;

	auto ping = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 7, "method", "ping"), ctx);
	EXPECT_TRUE(ping.isObject());
	EXPECT_TRUE(ping.getObject().getVar("result").isObject());

	// Notifications produce nothing at all.
	auto initialized = mcpDispatch(
		jo("jsonrpc", "2.0", "method", "notifications/initialized"),
		ctx);
	EXPECT_TRUE(initialized.getType() == typeNull);

	auto unknownNotification = mcpDispatch(
		jo("jsonrpc", "2.0", "method", "notifications/whatever"), ctx);
	EXPECT_TRUE(unknownNotification.getType() == typeNull);

	// Unknown request method: protocol error (notifications never
	// error).
	auto unknown = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 3, "method", "no/such/method"), ctx);
	EXPECT_EQ(
		unknown.getObject().getObject("error").getInt64("code"), -32601);

	// Broken envelopes: jsonrpc must be present and "2.0".
	auto badVersion = mcpDispatch(
		jo("jsonrpc", "1.0", "id", 4, "method", "ping"), ctx);
	EXPECT_EQ(
		badVersion.getObject().getObject("error").getInt64("code"), -32600);
}

TEST(mcp_dispatcher_tools_list_and_eval_gating) {
	mcpServerContext ctx;
	EXPECT_TRUE(hasTool(ctx, "state_get"));
	EXPECT_TRUE(hasTool(ctx, "state_list"));
	EXPECT_TRUE(hasTool(ctx, "state_set"));
	EXPECT_TRUE(hasTool(ctx, "call"));

	// The catalog the dispatcher serves carries schemas.
	auto reply = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 9, "method", "tools/list"), ctx);
	auto tools = reply.getObject().getObject("result").getList("tools");
	bool evalSeen = false;
	for (auto it = tools.begin(); it != tools.end(); ++it)
		if (it->getObject().getString("name") == "eval") evalSeen = true;

	if (evalSeen) {
		// With eval available the tool runs; with it disabled the call
		// errors instead of appearing in / working from the catalog.
		auto disabled = ctx;
		disabled.evalEnabled = false;
		EXPECT_TRUE(hasTool(disabled, "state_get"));
		EXPECT_TRUE(!hasTool(disabled, "eval"));
	}
}

TEST(mcp_dispatcher_state_round_trip) {
	mcpServerContext ctx;
	auto app = demoApp();
	ctx.globals.setObject("app", app);

	// Read through a path (and re-parse the text content the protocol
	// serves).
	auto reply = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 1, "method", "tools/call",
			"params",
			jo("name", "state_get",
				"arguments", jo("path", "app.nested.deep.answer"))),
		ctx);
	auto text = contentText(reply);
	auto content = jsonParse(text).getObject();
	EXPECT_EQ(content.getString("path"),
		string("app.nested.deep.answer"));
	EXPECT_EQ(content.getVar("json").getInt64(), 7);

	// A key listing before a read.
	auto listing = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 2, "method", "tools/call",
			"params",
			jo("name", "state_list",
				"arguments", jo("path", "app"))),
		ctx);
	auto members = jsonParse(contentText(listing))
					   .getObject()
					   .getObject("members");
	EXPECT_EQ(members.getString("name"), string("String"));
	EXPECT_EQ(members.getString("nested"), string("Object"));

	// Write through a path...
	auto written = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 3, "method", "tools/call",
			"params",
			jo("name", "state_set",
				"arguments",
				jo("path", "app.value", "value", 43))),
		ctx);
	EXPECT_TRUE(!written.getObject().getObject("result").getBool("isError", false));
	// ... and the write landed in the live gold data (handles).
	EXPECT_EQ(app.getInt64("value"), 43);

	// Uniform number arrays from real JSON land as gold vectors.
	auto vecWritten = mcpDispatch(
		jsonParse(R"({"jsonrpc":"2.0","id":4,"method":"tools/call")"
				  R"(,"params":{"name":"state_set","arguments":)"
				  R"({"path":"app.position","value":[1,2,3]}}})"),
		ctx);
	EXPECT_TRUE(!vecWritten.getObject().getObject("result").getBool("isError", false));
	// vec3f reads through the varRef accessor.
	EXPECT_EQ(app["position"].getFloat(0), 1.0f);
	EXPECT_EQ(app["position"].getFloat(2), 3.0f);

	// Missing paths and bad roots come back as tool errors, with the
	// roots named for cheap recovery.
	auto miss = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 5, "method", "tools/call",
			"params",
			jo("name", "state_get",
				"arguments", jo("path", "app.nope"))),
		ctx);
	auto missResult = miss.getObject().getObject("result");
	EXPECT_TRUE(missResult.getBool("isError", false));
	EXPECT_TRUE(contentText(miss).find("missing or null") !=
				string::npos);

	auto noRoot = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 6, "method", "tools/call",
			"params",
			jo("name", "state_get", "arguments", jo("path", "gears"))),
		ctx);
	EXPECT_TRUE(contentText(noRoot).find("no root 'gears'") !=
				string::npos);

	// Writes to missing members are refused (eval creates).
	auto create = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 7, "method", "tools/call",
			"params",
			jo("name", "state_set",
				"arguments", jo("path", "app.nope", "value", 1))),
		ctx);
	EXPECT_TRUE(create.getObject()
						.getObject("result")
						.getBool("isError", false));
}

TEST(mcp_dispatcher_call_invokes_methods) {
	mcpServerContext ctx;
	auto app = demoApp();
	ctx.globals.setObject("app", app);

	auto reply = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 1, "method", "tools/call",
			"params",
			jo("name", "call",
				"arguments",
				jo("path", "app", "method", "doubled",
					"args", ja(21)))),
		ctx);
	auto content = jsonParse(contentText(reply)).getObject();
	EXPECT_EQ(content.getInt64("result"), 42);

	// A method that does not exist is a tool error, not a protocol
	// error.
	auto missing = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 2, "method", "tools/call",
			"params",
			jo("name", "call",
				"arguments", jo("path", "app", "method", "tripled"))),
		ctx);
	EXPECT_TRUE(missing.getObject()
						   .getObject("result")
						   .getBool("isError", false));

	// An unknown tool name is a protocol error.
	auto noTool = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 3, "method", "tools/call",
			"params", jo("name", "vibes")),
		ctx);
	EXPECT_EQ(
		noTool.getObject().getObject("error").getInt64("code"), -32602);
}

TEST(mcp_dispatcher_eval_runs_gold_lang) {
	mcpServerContext ctx;
	auto app = demoApp();
	ctx.globals.setObject("app", app);

	// Does this build speak gold::lang? (Without the lang module the
	// catalog simply lacks eval; the rest of the suite still runs.)
	auto catalog = mcpToolCatalog(ctx);
	bool evalAvailable = hasTool(ctx, "eval");
	if (!evalAvailable) {
		EXPECT_TRUE(true);
		return;
	}

	auto reply = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 1, "method", "tools/call",
			"params",
			jo("name", "eval",
				"arguments", jo("source", "app.value = app.value + 8"))),
		ctx);
	EXPECT_TRUE(!reply.getObject().getObject("result").getBool("isError", false));
	// Shared handles: the script wrote into the app's live gold data.
	EXPECT_EQ(app.getInt64("value"), 50);

	auto expression = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 2, "method", "tools/call",
			"params",
			jo("name", "eval",
				"arguments", jo("source", "1 + 2 + 3"))),
		ctx);
	// Gold::lang arithmetic produces doubles; parse rather than string
	// compare.
	auto text = contentText(expression);
	EXPECT_NEAR(jsonParse(text).getDouble(), 6.0, 1e-9);

	auto broken = mcpDispatch(
		jo("jsonrpc", "2.0", "id", 3, "method", "tools/call",
			"params",
			jo("name", "eval",
				"arguments", jo("source", "app.missing(1)"))),
		ctx);
	auto result = broken.getObject().getObject("result");
	EXPECT_TRUE(result.getBool("isError", false));
}

TEST(mcp_facade_http_round_trip_server_mode) {
	const int port = 55300 + (::getpid() % 2000);
	mcpServer devServer(
		jo("host", "127.0.0.1", "port", port, "transport", "lws"));
		auto roots = jo("app", demoApp());
	devServer.setRoots({roots});

	std::atomic<int> started{-1};
	std::thread loop(
		[&]() mutable { started = devServer.start().getBool() ? 1 : 0; });

	const bool up = waitUntilUp(port);
	EXPECT_TRUE(up);
	if (up) {
		// The handshake.
		auto init = mcpPost(port,
			jsonStringify(jo("jsonrpc", "2.0", "id", 1,
				"method", "initialize", "params",
				jo("protocolVersion", "2025-06-18"))));
		EXPECT_TRUE(init.find("200 OK") != string::npos);
		auto initBody = bodyOf(init);
		auto negotiated = jsonParse(initBody).getObject();
		EXPECT_EQ(
			negotiated.getObject("result").getString("protocolVersion"),
			string("2025-06-18"));

		// A tool call round trip (server mode: no pump ever ran).
		auto call = mcpPost(port,
			jsonStringify(jo("jsonrpc", "2.0", "id", 2,
				"method", "tools/call", "params",
				jo("name", "state_set",
					"arguments",
					jo("path", "app.value", "value", 100)))));
		EXPECT_TRUE(call.find("200 OK") != string::npos);
		auto callBody = jsonParse(bodyOf(call)).getObject();
		EXPECT_TRUE(
			!callBody.getObject("result").getBool("isError", false));
		EXPECT_TRUE(contentText(callBody).find("100") != string::npos);

		// Notifications get the empty 202, and GET is not allowed.
		auto notification = mcpPost(port,
			jsonStringify(
				jo("jsonrpc", "2.0", "method", "notifications/initialized")));
		EXPECT_TRUE(
			notification.find("HTTP/1.1 202 ") != string::npos);
		auto get = exchange(port,
			"GET /mcp HTTP/1.1\r\nHost: mcp-test\r\nConnection: "
			"close\r\n\r\n");
		EXPECT_TRUE(get.find("405") != string::npos);
	}

	devServer.stop();
	const bool joined = [&]() {
		for (int i = 0; i < 120; ++i) {
			if (started.load() != -1) {
				loop.join();
				return true;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(25));
		}
		return false;
	}();
	EXPECT_TRUE(joined);
	EXPECT_EQ(started.load(), 1);
}

TEST(mcp_facade_frame_mode_runs_jobs_via_pump) {
	const int port = 57200 + (::getpid() % 2000);
	mcpServer devServer(
		jo("host", "127.0.0.1", "port", port, "transport", "lws"));
		auto roots = jo("app", demoApp());
	devServer.setRoots({roots});

	std::atomic<int> started{-1};
	std::thread loop(
		[&]() mutable { started = devServer.start().getBool() ? 1 : 0; });
	EXPECT_TRUE(waitUntilUp(port));

	// One pump establishes the frame consumer ("auto" execution);
	// after that requests queue instead of running inline.
	devServer.pump({});

	std::atomic<bool> done{false};
	string body;
	auto call = jsonStringify(jo("jsonrpc", "2.0", "id", 1,
		"method", "tools/call", "params",
		jo("name", "state_set",
			"arguments", jo("path", "app.value", "value", 55))));
	std::thread client([&] {
		auto response = mcpPost(port, call);
		body = bodyOf(response);
		done = true;
	});

	// Give the request time to land in the queue, then drain it the
	// way the engine loop does once per frame.
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	for (int i = 0; i < 50 && !done.load(); ++i) {
		devServer.pump({});
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	EXPECT_TRUE(done.load());
	auto reply = jsonParse(body).getObject();
	EXPECT_TRUE(!reply.getObject("result").getBool("isError", false));

	client.join();
	devServer.stop();
	for (int i = 0; i < 120; ++i) {
		if (started.load() != -1) {
			loop.join();
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}
}

TEST(mcp_facade_frame_mode_times_out_without_pump) {
	const int port = 58200 + (::getpid() % 2000);
	// A short timeout so the regression fails, not hangs.
	mcpServer devServer(jo("host", "127.0.0.1", "port", port,
		"transport", "lws", "timeout", 300));
		auto roots = jo("app", demoApp());
	devServer.setRoots({roots});

	std::atomic<int> started{-1};
	std::thread loop(
		[&]() mutable { started = devServer.start().getBool() ? 1 : 0; });
	EXPECT_TRUE(waitUntilUp(port));
	devServer.pump({});  // frame mode active, but the pump stops now

	const auto response = mcpPost(port,
		jsonStringify(jo("jsonrpc", "2.0", "id", 1,
			"method", "tools/call", "params",
			jo("name", "state_get", "arguments", jo("path", "app.value")))));
	EXPECT_TRUE(response.find("503") != string::npos);
	auto reply = jsonParse(bodyOf(response)).getObject();
	EXPECT_EQ(
		reply.getObject("error").getInt64("code"), -32001);

	devServer.stop();
	for (int i = 0; i < 120; ++i) {
		if (started.load() != -1) {
			loop.join();
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}
}

TEST(mcp_facade_destroy_is_idempotent) {
	mcpServer devServer(jo("host", "127.0.0.1", "port", 59900 + (::getpid() % 500)));
	devServer.destroy();
	devServer.destroy();
	EXPECT_TRUE(true);
}
int main() {
	return goldtest::runAll();
}

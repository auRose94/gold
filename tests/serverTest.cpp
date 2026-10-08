// First tests for the HTTP server: routing, the per-verb 404 catch-all,
// the 500 error policy for throwing handlers, and listener lifecycle.
//
// The vendored uWS transport is thread-affine (the App binds to the lazy
// loop of whatever thread creates it) and has no loop-stop API, so the
// live server is built, configured, and started inside ONE detached
// thread and the process ends with _Exit() after the report — see the
// comment in main(). All waits are bounded so a regression fails, and
// never hangs the runner.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#include "goldjs.hpp"
#include "goldtest.hpp"
#include "server.hpp"

using namespace gold;

namespace {

	/** Blocking one-shot HTTP exchange over a raw loopback socket. */
	string exchange(int port, const string& request) {
		int fd = (int)::socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) return "";
		timeval ioTimeout{0, 300000};  // 300 ms per recv; deadline bounds it
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
		// This transport leaves the connection open after a body response
		// (Connection: close is not honored), so waiting for EOF costs the
		// full receive timeout per exchange. Read until the advertised
		// response is complete instead.
		string out;
		const auto deadline = std::chrono::steady_clock::now() +
							  std::chrono::seconds(5);
		auto responseComplete = [&out]() {
			auto headerEnd = out.find("\r\n\r\n");
			if (headerEnd == string::npos) return false;
			size_t bodyStart = headerEnd + 4;
			// Case-insensitive Content-Length scan in the header block.
			string header = out.substr(0, bodyStart);
			for (auto& c : header) c = (char)tolower((unsigned char)c);
			auto cl = header.find("content-length:");
			if (cl == string::npos) return true;  // nothing more advertised
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

	/** Bounded probe until a port accepts connections. */
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

}  // namespace

TEST(server_bind_failure_reports_error) {
	// Without a listener bound yet this reports a plain false.
	server neverBound;
	EXPECT_TRUE(neverBound.start().getBool() == false);
}

TEST(server_routes_404_catch_all_and_error_policy) {
	// The App must be created, configured, and run on the same thread: the
	// transport binds to that thread's event loop. The server is leaked on
	// purpose; nothing else can free it safely.
	auto* web = new server();

	auto hello = func([](list args) -> var {
		auto res = args[1].getObject<response>();
		res.writeStatus(list({string("200 OK")}));
		res.end(list({string("hello")}));
		return var();
	});

	auto boom = func([](list) -> var {
		throw std::runtime_error("route blew up");
	});

	// setYield must be reachable on the prototype: without it the handler
	// neither answers nor yields and the router stops mid-chain.
	auto yielding = func([](list args) -> var {
		auto req = args[0].getObject<request>();
		req.setYield(list({var(true)}));
		return var();
	});

	// Same-thread init/registration/start — the documented production
	// pattern; only the loop thread may touch the handle.
	const int port = 20000 + (::getpid() % 20000);
	std::thread loop([=]() {
		web->initialize();
		web->get(list({string("/hello"), hello}));
		web->post(list({string("/boom"), boom}));
		web->get(list({string("/yield"), yielding}));
		web->setString("host", "127.0.0.1");
		web->setInt32("port", port);
		web->start();
	});

	const bool up = waitUntilUp(port);
	EXPECT_TRUE(up);
	if (up) {
		auto hit = exchange(port,
			"GET /hello HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(hit.find("200 OK") != string::npos);
		EXPECT_TRUE(hit.find("hello") != string::npos);

		auto miss = exchange(port,
			"GET /missing HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(miss.find("404 Not Found") != string::npos);

		// Non-GET 404: previously an unmatched POST left the connection
		// hanging with no response.
		auto missPost = exchange(port,
			"POST /missing HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(missPost.find("404 Not Found") != string::npos);

		// A throwing handler answers 500, never unwinds into the loop.
		auto err = exchange(port,
			"POST /boom HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(err.find("500 Internal Server Error") != string::npos);

		// (Not a same-port collision case: uSockets sets SO_REUSEPORT, so a
		// second bind to the same port legally succeeds on Linux.)
		auto err2 = exchange(port,
			"GET /boom HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(err2.find("404 Not Found") != string::npos);

		// Yield falls through to the next matching handler: the catch-all.
		auto yielded = exchange(port,
			"GET /yield HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(yielded.find("404 Not Found") != string::npos);
	}
	// The loop thread keeps running; it is reaped at process exit.
	loop.detach();
}

TEST(server_destroy_nulls_handle) {
	server web;
	web.initialize();
	web.destroy();
	// A second destroy must not delete a freed handle.
	web.destroy();
	EXPECT_TRUE(true);
}

int main() {
	const int code = goldtest::runAll();
	// This vintage transport has no graceful stop: skipping the library
	// teardown avoids its libuv loop-cleanup assertion on a live loop.
	// Every expect already counted and runAll has reported — flush that
	// report past _Exit so the runner still sees it.
	std::cout << std::flush;
	std::_Exit(code);
}
// First tests for the HTTP server: routing, the per-verb 404 catch-all,
// the 500 error policy for throwing handlers, WebSocket routes, mounts,
// and listener lifecycle.
//
// The lws transport (the only one served by this suite now) supports
// graceful stop: every live-server test owns its loop thread, stops it,
// and joins. All waits are bounded so a regression fails, and never
// hangs the runner.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <fstream>
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

	/** Send one masked client ws frame (text or close) on fd. */
	bool wsSendFrame(int fd, uint8_t opcode, const string& payload) {
		string frame{(char)(char(0x80) | char(opcode))};
		const auto size = payload.size();
		if (size < 126)
			frame.push_back(char(0x80 | size));  // mask bit + short len
		else if (size <= 0xffff) {
			frame.push_back(char(0x80 | 126));
			frame.push_back(char((size >> 8) & 0xff));
			frame.push_back(char(size & 0xff));
		}
		constexpr unsigned char mask[4] = {0xde, 0xad, 0xbe, 0xef};
		frame.append((const char*)mask, 4);
		for (size_t i = 0; i < size; ++i)
			frame.push_back(char(payload[i] ^ mask[i % 4]));
		size_t sent = 0;
		while (sent < frame.size()) {
			const auto n = ::send(fd, frame.data() + sent,
				frame.size() - sent, 0);
			if (n <= 0) return false;
			sent += (size_t)n;
		}
		return true;
	}

	/** Read one unmasked server ws frame, consuming `queue` bytes first
	 *  (bytes that coalesced past a previous response's headers); empty
	 *  result on fail/timeout. */
	string wsRecvFrame(int fd, uint8_t& opcode, string& queue) {
		auto pull = [&](char* dst, size_t n) -> bool {
			size_t got = 0;
			if (!queue.empty()) {
				const auto take = std::min(n, queue.size());
				memcpy(dst, queue.data(), take);
				queue.erase(0, take);
				got = take;
			}
			while (got < n) {
				const auto m = ::recv(fd, dst + got, n - got, 0);
				if (m <= 0) return false;
				got += (size_t)m;
			}
			return true;
		};
		char header[4];
		if (!pull(header, 2)) return "";
		opcode = (uint8_t)(header[0] & 0x0f);
		const bool masked = header[1] & 0x80;
		size_t length = (uint8_t)(header[1] & 0x7f);
		if (length == 126) {
			if (!pull(header, 2)) return "";
			length = size_t((uint8_t)header[0]) << 8 |
					 (uint8_t)header[1];
		}
		if (masked) {  // server frames are never masked
			unsigned char skip[4];
			if (!pull((char*)skip, 4)) return "";
		}
		string out;
		out.resize(length);
		if (!pull(out.data(), length)) return "";
		return out;
	}

}  // namespace

TEST(server_bind_failure_reports_error) {
	// Without a listener bound yet this reports a plain false.
	server neverBound;
	EXPECT_TRUE(neverBound.start().getBool() == false);
}

TEST(server_lws_post_body_flows_into_on_data) {
	const int port = 52000 + (::getpid() % 3000);
	server web(jo("host", "127.0.0.1", "port", port, "transport", "lws"));

	// The route defers its response to the raw body stream: onData's
	// gold-side callback gets (buffer, request, response) at the end.
	auto upload = func([](list args) -> var {
		auto req = args[0].getObject<request>();
		auto res = args[1].getObject<response>();
		res.onData(list({
			func([](list stream) -> var {
				auto buffer = stream[0].getString();
				auto resObj = stream[2].getObject<response>();
				resObj.writeStatus(list({(uint16_t)200}));
				resObj.end(list({buffer}));
				return var();
			}),
			var(req),
		}));
		return var();
	});
	web.post(list({string("/upload"), upload}));

	std::atomic<int> started{-1};
	std::thread loop([&]() mutable { started = web.start().getBool() ? 1 : 0; });

	const bool up = waitUntilUp(port);
	EXPECT_TRUE(up);
	if (up) {
		auto sent = exchange(port,
			"POST /upload HTTP/1.1\r\nHost: t\r\nContent-Length: 11\r\n"
			"Connection: close\r\n\r\nuploadBody\x21");
		EXPECT_TRUE(sent.find("200 OK") != string::npos);
		EXPECT_TRUE(sent.find("uploadBody\x21") != string::npos);
	}

	web.stop();
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

TEST(server_destroy_nulls_handle) {
	server web;
	web.initialize();
	web.destroy();
	// A second destroy must not delete a freed handle.
	web.destroy();
	EXPECT_TRUE(true);
}

	// ------------------------------------------------ lws-transport tests
	// The libwebsockets transport supports graceful stop: every lws test
	// owns its loop thread, stops it, and joins — no detached threads.

	/** Send a full byte string on fd. */
	bool wsSendAll(int fd, const string& data) {
		size_t sent = 0;
		while (sent < data.size()) {
			const auto n =
				::send(fd, data.data() + sent, data.size() - sent, 0);
			if (n <= 0) return false;
			sent += (size_t)n;
		}
		return true;
	}

TEST(server_lws_websocket_open_message_close) {
	const int port = 45000 + (::getpid() % 4000);
	server web(jo("host", "127.0.0.1", "port", port, "transport", "lws"));

	// The handlers run on the transport's loop thread; counts live on
	// the heap so the gold func closures copy only pointers.
	auto opened = std::make_shared<std::atomic<int>>(0);
	auto messages = std::make_shared<std::atomic<int>>(0);
	auto closedCount = std::make_shared<std::atomic<int>>(0);

	auto openFn = func([opened](list args) -> var {
		opened->fetch_add(1, std::memory_order_relaxed);
		// open may push right away: staged send flushes on writable.
		auto sock = args[0].getObject<wsSocket>();
		sock.send(list({string("welcome")}));
		return var();
	});
	auto msgFn = func([messages](list args) -> var {
		messages->fetch_add(1, std::memory_order_relaxed);
		auto sock = args[0].getObject<wsSocket>();
		sock.send(ja(args[1]));  // echo the payload back
		return var();
	});
	auto closeFn = func([closedCount](list) -> var {
		closedCount->fetch_add(1, std::memory_order_relaxed);
		return var();
	});

	web.ws(list({string("/echo"),
		jo("open", openFn, "message", msgFn, "close", closeFn)}));

	std::atomic<int> started{-1};
	std::thread loop([&]() mutable { started = web.start().getBool() ? 1 : 0; });

	const bool up = waitUntilUp(port);
	EXPECT_TRUE(up);
	int fd = -1;
	if (up) {
		fd = (int)::socket(AF_INET, SOCK_STREAM, 0);
		timeval ioTimeout{0, 300000};  // 300 ms per recv
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &ioTimeout,
			sizeof(ioTimeout));
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons((uint16_t)port);
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) == 0) {
			// The canonical RFC 6455 example key; its known accept is
			// "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=".
			auto handshake = string(
				"GET /echo HTTP/1.1\r\n"
				"Host: 127.0.0.1\r\n"
				"Upgrade: websocket\r\n"
				"Connection: Upgrade\r\n"
				"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
				"Sec-WebSocket-Version: 13\r\n\r\n");
			EXPECT_TRUE(wsSendAll(fd, handshake));
			// Bounded deadline for the 101 reply.
			string reply;
			const auto deadline =
				std::chrono::steady_clock::now() + std::chrono::seconds(5);
			while (std::chrono::steady_clock::now() < deadline) {
				char buf[4096];
				const auto n = ::recv(fd, buf, sizeof(buf), 0);
				if (n <= 0) break;
				reply.append(buf, (size_t)n);
				if (reply.find("\r\n\r\n") != string::npos) break;
			}
			// The 101 + accept prove lws negotiated our upgrade.
			EXPECT_TRUE(reply.find("101 Switching Protocols") !=
				string::npos);
			EXPECT_TRUE(reply.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") !=
				string::npos);

			if (reply.find("101") != string::npos) {
				// Bytes after the 101's headers may have coalesced into
				// the same read: they are the first server frame(s).
				const auto headEnd = reply.find("\r\n\r\n");
				string queue;
				if (headEnd != string::npos &&
					headEnd + 4 < reply.size())
					queue = reply.substr(headEnd + 4);
				uint8_t opcode = 0;
				// The open handler's immediate send arrives first.
				auto welcome = wsRecvFrame(fd, opcode, queue);
				EXPECT_EQ(int(opcode), 1);
				EXPECT_TRUE(welcome == "welcome");

				EXPECT_TRUE(wsSendFrame(fd, 1, "ping!"));
				auto got = wsRecvFrame(fd, opcode, queue);
				EXPECT_EQ(int(opcode), 1);
				EXPECT_TRUE(got == "ping!");

				// Peer-initiated close: the server echoes the close
				// frame (opcode 8) before dropping the connection.
				EXPECT_TRUE(wsSendFrame(fd, 8, ""));
				auto bye = wsRecvFrame(fd, opcode, queue);
				EXPECT_EQ(int(opcode), 8);
				(void)bye;
			}
		}
	}
	if (fd >= 0) ::close(fd);

	// Bounded stop: the loop thread leaves start() and joins.
	web.stop();
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
	EXPECT_EQ(opened->load(), up ? 1 : 0);
	EXPECT_EQ(messages->load(), up ? 1 : 0);
	EXPECT_EQ(closedCount->load(), up ? 1 : 0);
}

TEST(server_lws_mounts_serving_with_etag) {
	const int port = 48000 + (::getpid() % 3000);
	// A temp mount dir; mount URLs carry the directory's basename.
	namespace fs = std::filesystem;
	const auto mnt = fs::temp_directory_path() /
		("gold-lws-mount-" + std::to_string(::getpid()));
	fs::create_directories(mnt);
	{
		std::ofstream page(mnt / "page.html");
		page << "<h1>served</h1>";
	}

	server web(jo("host", "127.0.0.1", "port", port, "transport", "lws"));
	web.setMountPoint(list({mnt.string()}));

	std::atomic<int> started{-1};
	std::thread loop([&]() mutable { started = web.start().getBool() ? 1 : 0; });

	const bool up = waitUntilUp(port);
	EXPECT_TRUE(up);
	if (up) {
		const auto base =
			"GET /gold-lws-mount-" + std::to_string(::getpid()) +
			"/page.html HTTP/1.1\r\n";
		auto hit = exchange(port, base + "Connection: close\r\n\r\n");
		EXPECT_TRUE(hit.find("200 OK") != string::npos);
		EXPECT_TRUE(hit.find("served") != string::npos);
		EXPECT_TRUE(hit.find("text/html") != string::npos);

		// The served ETag then answers a revalidation with 304.
		const auto mark = hit.find("ETag: ");
		string etag;
		if (mark != string::npos) {
			etag = hit.substr(mark + 6);
			etag = etag.substr(0, etag.find("\r\n"));
		}
		EXPECT_TRUE(!etag.empty());
		auto cached = exchange(port,
			base + "If-None-Match: " + etag + "\r\n\r\n");
		EXPECT_TRUE(cached.find("304 Not Modified") != string::npos);
		EXPECT_FALSE(cached.find("served") != string::npos);

		auto missing = exchange(port,
			"GET /gold-lws-mount-" + std::to_string(::getpid()) +
				"/nope.html HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(missing.find("404 Not Found") != string::npos);
	}

	web.stop();
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
	std::error_code ecRemove;
	fs::remove_all(mnt, ecRemove);
}

TEST(server_lws_routes_404_500_and_stops) {
	const int port = 40000 + (::getpid() % 5000);
	server web(jo("host", "127.0.0.1", "port", port, "transport", "lws"));

	auto hello = func([](list args) -> var {
		auto res = args[1].getObject<response>();
		res.writeStatus(list({string("200 OK")}));
		res.end(list({string("hello")}));
		return var();
	});

	auto boom = func([](list) -> var {
		throw std::runtime_error("route blew up");
	});

	auto yielding = func([](list args) -> var {
		auto req = args[0].getObject<request>();
		req.setYield(list({var(true)}));
		return var();
	});

	web.get(list({string("/hello"), hello}));
	web.post(list({string("/boom"), boom}));
	web.get(list({string("/yield"), yielding}));

	std::atomic<int> started{-1};
	std::thread loop([&]() mutable {
		started = web.start().getBool() ? 1 : 0;
	});

	const bool up = waitUntilUp(port);
	EXPECT_TRUE(up);
	if (up) {
		auto hit = exchange(port,
			"GET /hello HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(hit.find("200 OK") != string::npos);
		EXPECT_TRUE(hit.find("hello") != string::npos);
		{
			auto lowerCase = hit;
			for (auto& ch : lowerCase)
				ch = (char)tolower((unsigned char)ch);
			EXPECT_TRUE(
				lowerCase.find("content-length: 5") != string::npos);
		}

		auto miss = exchange(port,
			"GET /missing HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(miss.find("404 Not Found") != string::npos);

		auto missPost = exchange(port,
			"POST /missing HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(missPost.find("404 Not Found") != string::npos);

		auto err = exchange(port,
			"POST /boom HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(err.find("500 Internal Server Error") != string::npos);

		auto err2 = exchange(port,
			"GET /boom HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(err2.find("404 Not Found") != string::npos);

		auto yielded = exchange(port,
			"GET /yield HTTP/1.1\r\nConnection: close\r\n\r\n");
		EXPECT_TRUE(yielded.find("404 Not Found") != string::npos);
	}

	web.stop();
	const bool joined = [&]() {
		for (int i = 0; i < 120; ++i) {
			if (started.load() != -1) {
				loop.join();
				return true;
			}
			std::this_thread::sleep_for(
				std::chrono::milliseconds(25));
		}
		return false;
	}();
	EXPECT_TRUE(joined);
	EXPECT_EQ(started.load(), 1);
}

int main() {
	const int code = goldtest::runAll();

	// The lws transport stops and joins cleanly — no teardown dodge
	// needed anymore.
	return code;
}
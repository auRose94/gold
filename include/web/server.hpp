#pragma once

#include "types.hpp"

#define serveArgs(args, req, res)          \
	auto req = args[0].getObject<request>(); \
	auto res = args[1].getObject<response>();

namespace gold {
	struct request;
	struct response;
	struct session;
	struct wsConn;

	/** The status line for a status code ("200 OK", "404 Not Found"); shared
	 *  by transports when assembling raw responses. */
	string httpStatusLine(uint16_t code);

	/** The HTTP server facade, transport-independent: routes are buffered
	 *  as gold data ("routes": verb -> {pattern -> func}), and start()
	 *  resolves a serverTransport (config "transport"; the loadable
	 *  libgoldLws plugin) whose run() consumes everything. */
	struct server : public object {
	 protected:
		static object& getPrototype();

	 public:
		server();
		server(object config);

		/** Blocking: resolves the transport and runs it. Returns an error
		 *  when the bind fails instead of entering the loop. */
		var start(list args = {});
		/** Request a graceful stop of a running start(); safe from any
		 *  thread. Returns false when nothing is running. */
		var stop(list args = {});
		/** Buffer a WebSocket route: `ws(pattern, {open, message,
		 *  close})`. open(sock) fires when a client connects,
		 *  message(sock, data) per message, close(sock) on teardown. */
		var ws(list args);
		var get(list args);
		var post(list args);
		var put(list args);
		var patch(list args);
		var del(list args);
		var options(list args);
		var setMountPoint(list args);
		var setErrorHandler(list args);
		var initialize(list args = {});
		var destroy(list args = {});
	};

	/** A request: gold data (headers/params/query/method/path) staged by
	 *  the transport that accepted the connection. */
	struct request : public object {
	 public:
		static object& getPrototype();
		var getAllHeaders(list args = {});

		var getHeader(list args);
		var getMethod(list args = {});
		var getParameter(list args);
		var getQuery(list args = {});
		var getUrl(list args = {});
		var getYield(list args = {});
		var setYield(list args);

		bool isWWWFormURLEncoded();
		bool isJSON();
		bool acceptingJSON();
		bool acceptingHTML();

		request();
		request(session* s);
	};

	/** A response: staging (status/headers/body typing) in gold data,
	 *  raw writes delegated to the connection's session. */
	struct response : public object {
	 public:
		static object& getPrototype();
		response();
		response(session* s);

		var writeContinue(list args = {});
		var writeStatus(list args);
		var writeHeader(list args);
		var end(list args = {});
		var tryEnd(list args = {});
		var write(list args);
		var getWriteOffset(list args = {});
		var hasResponded(list args = {});
		var cork(list args);
		var onWritable(list args);
		var onAborted(list args);
		var onData(list args);
	};

	/** A WebSocket connection handed to a server.ws() handler. Sends
	 *  (text or binary) stage and flush on the connection's writable
	 *  moment; close() requests a graceful close. */
	struct wsSocket : public object {
	 public:
		static object& getPrototype();
		wsSocket();
		wsSocket(wsConn* c, const string& path);

		var send(list args);
		var close(list args = {});
	};
}  // namespace gold
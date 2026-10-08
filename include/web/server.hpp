#pragma once

#include "types.hpp"

#define serveArgs(args, req, res)          \
	auto req = args[0].getObject<request>(); \
	auto res = args[1].getObject<response>();

namespace gold {
	struct request;
	struct response;
	struct session;

	/**
	 * The HTTP server facade, transport-independent: routes are buffered
	 * as gold data ("routes": verb -> {pattern -> func}), and start()
	 * resolves a serverTransport (config "transport"; a loadable plugin
	 * like libgoldUws or libgoldLws) whose run() consumes everything.
	 */
	struct server : public object {
	 protected:
		static object& getPrototype();

	 public:
		server();
		server(object config);

		/** Blocking: resolves the transport and runs it. Returns an error
		 *  when the bind fails instead of entering the loop. */
		var start(list args = {});
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
}  // namespace gold